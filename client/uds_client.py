#!/usr/bin/env python3
"""Dependency-free UDS/ISO-TP client for the virtual SocketCAN demo server.

This intentionally implements ISO-TP in Python instead of opening a kernel
CAN_ISOTP socket.  Raw frames remain visible to candump and the Qt monitor,
which makes the protocol state machine easier to study.
"""

from __future__ import annotations

import argparse
import cmd
import dataclasses
import socket
import struct
import sys
import time
from typing import Iterable, Optional, Sequence


POSITIVE_RESPONSE_OFFSET = 0x40
NEGATIVE_RESPONSE_SID = 0x7F

SERVICES = {
    0x10: "DiagnosticSessionControl",
    0x11: "ECUReset",
    0x14: "ClearDiagnosticInformation",
    0x19: "ReadDTCInformation",
    0x22: "ReadDataByIdentifier",
    0x27: "SecurityAccess",
    0x28: "CommunicationControl",
    0x2E: "WriteDataByIdentifier",
    0x31: "RoutineControl",
    0x3E: "TesterPresent",
}

NRC_NAMES = {
    0x10: "generalReject",
    0x11: "serviceNotSupported",
    0x12: "subFunctionNotSupported",
    0x13: "incorrectMessageLengthOrInvalidFormat",
    0x14: "responseTooLong",
    0x21: "busyRepeatRequest",
    0x22: "conditionsNotCorrect",
    0x24: "requestSequenceError",
    0x31: "requestOutOfRange",
    0x33: "securityAccessDenied",
    0x35: "invalidKey",
    0x36: "exceededNumberOfAttempts",
    0x37: "requiredTimeDelayNotExpired",
    0x70: "uploadDownloadNotAccepted",
    0x72: "generalProgrammingFailure",
    0x78: "responsePending",
    0x7E: "subFunctionNotSupportedInActiveSession",
    0x7F: "serviceNotSupportedInActiveSession",
}

DID_NAMES = {
    0xF190: "VIN",
    0xF195: "softwareVersion",
    0xF18C: "ecuSerialNumber",
    0xF187: "sparePartNumber",
    0xF1A0: "workshopFingerprint",
    0x0100: "supplyVoltageMv",
}


class UdsError(RuntimeError):
    """Base client error."""


class UdsTimeout(UdsError):
    """A transport response did not arrive in time."""


class IsoTpError(UdsError):
    """An ISO-TP frame or transition was invalid."""


class NegativeResponse(UdsError):
    def __init__(self, service: int, code: int):
        self.service = service
        self.code = code
        name = NRC_NAMES.get(code, "unknownNrc")
        super().__init__(f"{SERVICES.get(service, hex(service))} rejected: {name} (0x{code:02X})")


@dataclasses.dataclass(frozen=True)
class CanFrame:
    arbitration_id: int
    data: bytes


class RawCanSocket:
    """Small Linux SocketCAN wrapper using the classic 16-byte can_frame ABI."""

    _FRAME = struct.Struct("=IB3x8s")
    _CAN_EFF_FLAG = 0x80000000
    _CAN_EFF_MASK = 0x1FFFFFFF

    def __init__(self, interface: str, *, verbose: bool = False):
        if not hasattr(socket, "AF_CAN"):
            raise UdsError("Python was built without SocketCAN support; run this client on Linux")
        self.interface = interface
        self.verbose = verbose
        self._socket = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
        self._socket.bind((interface,))

    def close(self) -> None:
        self._socket.close()

    def __enter__(self) -> "RawCanSocket":
        return self

    def __exit__(self, *_: object) -> None:
        self.close()

    def send(self, frame: CanFrame) -> None:
        if len(frame.data) > 8:
            raise IsoTpError("classic CAN frames cannot exceed eight bytes")
        can_id = frame.arbitration_id
        if can_id > 0x7FF:
            can_id |= self._CAN_EFF_FLAG
        data = frame.data.ljust(8, b"\x00")
        self._socket.send(self._FRAME.pack(can_id, len(data), data))
        if self.verbose:
            print(f"CAN TX {frame.arbitration_id:03X}  {data.hex(' ').upper()}")

    def receive(self, timeout: float) -> CanFrame:
        self._socket.settimeout(timeout)
        try:
            packet = self._socket.recv(self._FRAME.size)
        except socket.timeout as error:
            raise UdsTimeout(f"no CAN frame received within {timeout:.3f} s") from error
        can_id, length, raw = self._FRAME.unpack(packet)
        frame = CanFrame(can_id & self._CAN_EFF_MASK, raw[: min(length, 8)])
        if self.verbose:
            print(f"CAN RX {frame.arbitration_id:03X}  {frame.data.hex(' ').upper()}")
        return frame


class IsoTpTransport:
    """ISO 15765-2 normal-addressing transport over a RawCanSocket."""

    def __init__(
        self,
        bus: RawCanSocket,
        transmit_id: int = 0x7E0,
        receive_id: int = 0x7E8,
        timeout: float = 1.0,
        block_size: int = 0,
        st_min: int = 0,
    ):
        self.bus = bus
        self.transmit_id = transmit_id
        self.receive_id = receive_id
        self.timeout = timeout
        self.block_size = block_size
        self.st_min = st_min

    def _send_frame(self, data: bytes) -> None:
        self.bus.send(CanFrame(self.transmit_id, data.ljust(8, b"\x00")))

    def _receive_frame(self, deadline: float) -> bytes:
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise UdsTimeout("ISO-TP receive deadline expired")
            frame = self.bus.receive(remaining)
            if frame.arbitration_id == self.receive_id:
                return frame.data

    @staticmethod
    def _separation_seconds(st_min: int) -> float:
        if st_min <= 0x7F:
            return st_min / 1000.0
        if 0xF1 <= st_min <= 0xF9:
            return (st_min - 0xF0) / 10_000.0
        return 0.0

    def send(self, payload: bytes) -> None:
        if not 1 <= len(payload) <= 4095:
            raise IsoTpError("payload length must be between 1 and 4095 bytes")
        if len(payload) <= 7:
            self._send_frame(bytes([len(payload)]) + payload)
            return

        self._send_frame(bytes([0x10 | (len(payload) >> 8), len(payload) & 0xFF]) + payload[:6])
        offset = 6
        sequence = 1
        block_count = 0
        block_size = 0
        separation = 0.0

        while offset < len(payload):
            if block_count == 0:
                deadline = time.monotonic() + self.timeout
                while True:
                    flow = self._receive_frame(deadline)
                    if flow[0] >> 4 != 0x3:
                        continue
                    status = flow[0] & 0x0F
                    if status == 0:
                        block_size = flow[1]
                        separation = self._separation_seconds(flow[2])
                        break
                    if status == 2:
                        raise IsoTpError("receiver sent flow-control overflow")
                    # WAIT: keep listening until the original deadline.

            chunk = payload[offset : offset + 7]
            self._send_frame(bytes([0x20 | sequence]) + chunk)
            offset += len(chunk)
            sequence = (sequence + 1) & 0x0F
            block_count += 1
            if offset < len(payload) and separation:
                time.sleep(separation)
            if block_size and block_count >= block_size:
                block_count = 0

    def receive(self) -> bytes:
        deadline = time.monotonic() + self.timeout
        first = self._receive_frame(deadline)
        frame_type = first[0] >> 4
        if frame_type == 0:
            length = first[0] & 0x0F
            if not 1 <= length <= 7 or len(first) < length + 1:
                raise IsoTpError("malformed single frame")
            return first[1 : 1 + length]
        if frame_type != 1 or len(first) < 8:
            raise IsoTpError(f"expected first or single frame, got PCI 0x{first[0]:02X}")

        expected = ((first[0] & 0x0F) << 8) | first[1]
        if not 8 <= expected <= 4095:
            raise IsoTpError(f"invalid first-frame length {expected}")
        output = bytearray(first[2:])
        self._send_frame(bytes([0x30, self.block_size, self.st_min]))
        sequence = 1
        block_count = 0

        while len(output) < expected:
            frame = self._receive_frame(time.monotonic() + self.timeout)
            if frame[0] >> 4 != 0x2:
                raise IsoTpError(f"expected consecutive frame, got PCI 0x{frame[0]:02X}")
            if frame[0] & 0x0F != sequence:
                raise IsoTpError(f"sequence mismatch: expected {sequence:X}, got {frame[0] & 0x0F:X}")
            output.extend(frame[1:])
            sequence = (sequence + 1) & 0x0F
            block_count += 1
            if self.block_size and block_count >= self.block_size and len(output) < expected:
                self._send_frame(bytes([0x30, self.block_size, self.st_min]))
                block_count = 0
        return bytes(output[:expected])


class UdsClient:
    def __init__(self, transport: IsoTpTransport, *, verbose: bool = False):
        self.transport = transport
        self.verbose = verbose

    def request(self, service: int, data: bytes = b"") -> bytes:
        request = bytes([service]) + data
        if self.verbose:
            print(f"UDS TX {request.hex(' ').upper()}")
        self.transport.send(request)
        while True:
            response = self.transport.receive()
            if self.verbose:
                print(f"UDS RX {response.hex(' ').upper()}")
            if len(response) >= 3 and response[0] == NEGATIVE_RESPONSE_SID:
                if response[2] == 0x78:
                    continue
                raise NegativeResponse(response[1], response[2])
            if not response or response[0] != ((service + POSITIVE_RESPONSE_OFFSET) & 0xFF):
                raise UdsError(f"unexpected response to 0x{service:02X}: {response.hex(' ')}")
            return response[1:]

    def change_session(self, session: int) -> tuple[int, int]:
        response = self.request(0x10, bytes([session]))
        if len(response) != 5 or response[0] != session:
            raise UdsError("malformed session-control response")
        p2_ms = int.from_bytes(response[1:3], "big")
        p2_star_ms = int.from_bytes(response[3:5], "big") * 10
        return p2_ms, p2_star_ms

    def ecu_reset(self, reset_type: int = 1) -> bytes:
        return self.request(0x11, bytes([reset_type]))

    @staticmethod
    def calculate_key(seed: int, level: int = 1) -> int:
        rotation = (level * 5) & 31
        rotated = ((seed << rotation) | (seed >> (32 - rotation))) & 0xFFFFFFFF
        mixed = rotated ^ ((0xA5C31F27 + level * 0x01020304) & 0xFFFFFFFF)
        return (mixed + 0x13579BDF) & 0xFFFFFFFF

    def unlock(self, level: int = 1) -> int:
        if level not in (1, 2):
            raise ValueError("the demo supports security levels 1 and 2")
        seed_subfunction = level * 2 - 1
        seed_response = self.request(0x27, bytes([seed_subfunction]))
        if len(seed_response) != 5 or seed_response[0] != seed_subfunction:
            raise UdsError("malformed seed response")
        seed = int.from_bytes(seed_response[1:], "big")
        if seed == 0:
            return 0
        key = self.calculate_key(seed, level)
        self.request(0x27, bytes([seed_subfunction + 1]) + key.to_bytes(4, "big"))
        return key

    def read_dids(self, dids: Iterable[int]) -> bytes:
        encoded = b"".join(did.to_bytes(2, "big") for did in dids)
        return self.request(0x22, encoded)

    def write_did(self, did: int, value: bytes) -> None:
        response = self.request(0x2E, did.to_bytes(2, "big") + value)
        if response != did.to_bytes(2, "big"):
            raise UdsError("write response did not echo the DID")

    def routine(self, control_type: int, routine_id: int, option_record: bytes = b"") -> bytes:
        return self.request(0x31, bytes([control_type]) + routine_id.to_bytes(2, "big") + option_record)

    def read_dtcs(self, status_mask: int = 0xFF) -> list[tuple[int, int]]:
        response = self.request(0x19, bytes([0x02, status_mask]))
        if len(response) < 2 or response[0] != 0x02:
            raise UdsError("malformed DTC response")
        records = []
        for offset in range(2, len(response), 4):
            if offset + 4 <= len(response):
                records.append((int.from_bytes(response[offset : offset + 3], "big"), response[offset + 3]))
        return records

    def clear_dtcs(self, group: int = 0xFFFFFF) -> None:
        self.request(0x14, group.to_bytes(3, "big"))

    def tester_present(self) -> None:
        self.request(0x3E, b"\x00")


def integer(text: str, *, bits: int = 32) -> int:
    value = int(text, 0)
    if not 0 <= value < (1 << bits):
        raise argparse.ArgumentTypeError(f"value must fit in {bits} bits")
    return value


def hex_bytes(parts: Sequence[str]) -> bytes:
    try:
        normalized = "".join(parts).replace("0x", "").replace(":", "").replace(" ", "")
        return bytes.fromhex(normalized)
    except ValueError as error:
        raise UdsError(f"invalid hexadecimal bytes: {' '.join(parts)}") from error


def printable(value: bytes) -> str:
    text = "".join(chr(byte) if 32 <= byte < 127 else "." for byte in value)
    return f"{value.hex(' ').upper()}  |{text}|"


class DiagnosticShell(cmd.Cmd):
    intro = "Interactive UDS shell. Type help or ? to list commands."
    prompt = "uds> "

    def __init__(self, client: UdsClient):
        super().__init__()
        self.client = client

    def do_session(self, argument: str) -> None:
        "session <1|2|3> -- change diagnostic session"
        p2, p2_star = self.client.change_session(integer(argument, bits=8))
        print(f"session active; P2={p2} ms, P2*={p2_star} ms")

    def do_unlock(self, argument: str) -> None:
        "unlock [level] -- perform the demo seed/key exchange"
        level = integer(argument, bits=8) if argument.strip() else 1
        print(f"unlocked level {level}; key=0x{self.client.unlock(level):08X}")

    def do_read(self, argument: str) -> None:
        "read <DID> [DID ...] -- read one or more data identifiers"
        dids = [integer(item, bits=16) for item in argument.split()]
        print(printable(self.client.read_dids(dids)))

    def do_write(self, argument: str) -> None:
        "write <DID> <text> -- write an ASCII value"
        did, separator, value = argument.partition(" ")
        if not separator:
            raise UdsError("usage: write <DID> <text>")
        self.client.write_did(integer(did, bits=16), value.encode())
        print("write accepted")

    def do_dtc(self, argument: str) -> None:
        "dtc [status-mask] -- list DTCs"
        mask = integer(argument, bits=8) if argument.strip() else 0xFF
        for code, status in self.client.read_dtcs(mask):
            print(f"{code:06X} status=0x{status:02X}")

    def do_raw(self, argument: str) -> None:
        "raw <SID> [hex bytes] -- send an arbitrary UDS request"
        fields = argument.split()
        if not fields:
            raise UdsError("usage: raw <SID> [hex bytes]")
        print(printable(self.client.request(integer(fields[0], bits=8), hex_bytes(fields[1:]))))

    def do_tester(self, _: str) -> None:
        "tester -- send TesterPresent"
        self.client.tester_present()
        print("present")

    def do_quit(self, _: str) -> bool:
        "quit -- close the shell"
        return True

    do_exit = do_quit

    def default(self, line: str) -> None:
        try:
            super().default(line)
        except UdsError as error:
            print(f"error: {error}")

    def onecmd(self, line: str) -> bool:
        try:
            return super().onecmd(line)
        except (UdsError, ValueError) as error:
            print(f"error: {error}")
            return False


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="UDS over raw SocketCAN test client")
    parser.add_argument("-i", "--interface", default="vcan0")
    parser.add_argument("--request-id", type=lambda x: integer(x, bits=29), default=0x7E0)
    parser.add_argument("--response-id", type=lambda x: integer(x, bits=29), default=0x7E8)
    parser.add_argument("--timeout", type=float, default=1.0)
    parser.add_argument("-v", "--verbose", action="store_true")
    sub = parser.add_subparsers(dest="command", required=True)

    session = sub.add_parser("session", help="change diagnostic session")
    session.add_argument("session", type=lambda x: integer(x, bits=8), choices=(1, 2, 3))
    unlock = sub.add_parser("unlock", help="perform seed/key security access")
    unlock.add_argument("level", type=int, nargs="?", default=1, choices=(1, 2))
    read = sub.add_parser("read", help="read one or more DIDs")
    read.add_argument("dids", nargs="+", type=lambda x: integer(x, bits=16))
    write = sub.add_parser("write", help="write a DID")
    write.add_argument("did", type=lambda x: integer(x, bits=16))
    write.add_argument("value", help="ASCII text, or hex bytes with --hex")
    write.add_argument("--hex", action="store_true")
    routine = sub.add_parser("routine", help="control a routine")
    routine.add_argument("type", type=lambda x: integer(x, bits=8), choices=(1, 2, 3))
    routine.add_argument("id", type=lambda x: integer(x, bits=16))
    routine.add_argument("option", nargs="*", default=[])
    dtc = sub.add_parser("dtc", help="report DTCs by status mask")
    dtc.add_argument("--mask", type=lambda x: integer(x, bits=8), default=0xFF)
    clear = sub.add_parser("clear", help="clear a DTC group")
    clear.add_argument("group", nargs="?", type=lambda x: integer(x, bits=24), default=0xFFFFFF)
    reset = sub.add_parser("reset", help="request an ECU reset")
    reset.add_argument("type", nargs="?", type=lambda x: integer(x, bits=8), default=1)
    sub.add_parser("tester", help="send TesterPresent")
    raw = sub.add_parser("raw", help="send a raw UDS service request")
    raw.add_argument("sid", type=lambda x: integer(x, bits=8))
    raw.add_argument("data", nargs="*")
    sub.add_parser("demo", help="run an end-to-end demonstration")
    sub.add_parser("shell", help="start an interactive diagnostic shell")
    sub.add_parser("key", help="run local seed/key known-answer tests")
    return parser


def run_demo(client: UdsClient) -> None:
    p2, p2_star = client.change_session(3)
    print(f"Extended session: P2={p2} ms P2*={p2_star} ms")
    key = client.unlock(1)
    print(f"Security level 1 unlocked (key 0x{key:08X})")
    print(f"VIN/software response: {printable(client.read_dids([0xF190, 0xF195]))}")
    client.write_did(0xF1A0, b"CODEX-WORKSHOP")
    print(f"Fingerprint response: {printable(client.read_dids([0xF1A0]))}")
    started = client.routine(1, 0xFF00, b"\xCA\xFE")
    print(f"Routine started: {started.hex(' ').upper()}")
    time.sleep(0.12)
    result = client.routine(3, 0xFF00)
    print(f"Routine result:  {result.hex(' ').upper()}")
    for code, status in client.read_dtcs():
        print(f"DTC {code:06X} status=0x{status:02X}")


def run_key_self_test() -> None:
    vectors = {
        (0x00000001, 1): 0xBA1CBDEA,
        (0x12345678, 1): 0xF3A78908,
        (0xDEADBEEF, 2): 0x24943634,
    }
    for (seed, level), expected in vectors.items():
        actual = UdsClient.calculate_key(seed, level)
        if actual != expected:
            raise AssertionError(f"key vector failed: {seed:08X}/{level}: {actual:08X} != {expected:08X}")
        print(f"seed=0x{seed:08X} level={level} key=0x{actual:08X}")


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = build_parser().parse_args(argv)
    if args.command == "key":
        run_key_self_test()
        return 0

    try:
        with RawCanSocket(args.interface, verbose=args.verbose) as bus:
            transport = IsoTpTransport(bus, args.request_id, args.response_id, args.timeout)
            client = UdsClient(transport, verbose=args.verbose)
            if args.command == "session":
                p2, p2_star = client.change_session(args.session)
                print(f"session 0x{args.session:02X}; P2={p2} ms P2*={p2_star} ms")
            elif args.command == "unlock":
                print(f"key=0x{client.unlock(args.level):08X}")
            elif args.command == "read":
                print(printable(client.read_dids(args.dids)))
            elif args.command == "write":
                value = hex_bytes([args.value]) if args.hex else args.value.encode()
                client.write_did(args.did, value)
                print("write accepted")
            elif args.command == "routine":
                print(printable(client.routine(args.type, args.id, hex_bytes(args.option))))
            elif args.command == "dtc":
                for code, status in client.read_dtcs(args.mask):
                    print(f"{code:06X} status=0x{status:02X}")
            elif args.command == "clear":
                client.clear_dtcs(args.group)
                print("DTC group cleared")
            elif args.command == "reset":
                print(printable(client.ecu_reset(args.type)))
            elif args.command == "tester":
                client.tester_present()
                print("tester present acknowledged")
            elif args.command == "raw":
                print(printable(client.request(args.sid, hex_bytes(args.data))))
            elif args.command == "demo":
                run_demo(client)
            elif args.command == "shell":
                DiagnosticShell(client).cmdloop()
        return 0
    except (OSError, UdsError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

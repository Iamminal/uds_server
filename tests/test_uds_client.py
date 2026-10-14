import importlib.util
import pathlib
import sys
import unittest


CLIENT_PATH = pathlib.Path(__file__).parents[1] / "client" / "uds_client.py"
SPEC = importlib.util.spec_from_file_location("uds_client", CLIENT_PATH)
uds_client = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = uds_client
SPEC.loader.exec_module(uds_client)


class FakeBus:
    def __init__(self, incoming=()):
        self.incoming = list(incoming)
        self.sent = []

    def send(self, frame):
        self.sent.append(frame)

    def receive(self, _timeout):
        if not self.incoming:
            raise uds_client.UdsTimeout("fake bus empty")
        return self.incoming.pop(0)


class IsoTpTests(unittest.TestCase):
    def test_single_frame_send(self):
        bus = FakeBus()
        transport = uds_client.IsoTpTransport(bus)
        transport.send(bytes.fromhex("22 F1 90"))
        self.assertEqual(bus.sent[0].arbitration_id, 0x7E0)
        self.assertEqual(bus.sent[0].data[:4], bytes.fromhex("03 22 F1 90"))

    def test_multi_frame_send_honors_flow_control(self):
        flow_control = uds_client.CanFrame(0x7E8, bytes.fromhex("30 00 00 00 00 00 00 00"))
        bus = FakeBus([flow_control])
        transport = uds_client.IsoTpTransport(bus)
        transport.send(bytes(range(1, 20)))
        self.assertEqual(bus.sent[0].data[:2], bytes.fromhex("10 13"))
        self.assertEqual(bus.sent[1].data[0], 0x21)
        self.assertEqual(bus.sent[2].data[0], 0x22)

    def test_multi_frame_receive_sends_flow_control(self):
        payload = bytes.fromhex("62 F1 90") + b"WVWZZZ1JZXW000001"
        frames = [
            uds_client.CanFrame(0x7E8, bytes([0x10, len(payload)]) + payload[:6]),
            uds_client.CanFrame(0x7E8, bytes([0x21]) + payload[6:13]),
            uds_client.CanFrame(0x7E8, bytes([0x22]) + payload[13:20]),
        ]
        bus = FakeBus(frames)
        transport = uds_client.IsoTpTransport(bus)
        self.assertEqual(transport.receive(), payload)
        self.assertEqual(bus.sent[0].data[:3], bytes.fromhex("30 00 00"))

    def test_key_vectors(self):
        self.assertEqual(uds_client.UdsClient.calculate_key(0x00000001, 1), 0xBA1CBDEA)
        self.assertEqual(uds_client.UdsClient.calculate_key(0xDEADBEEF, 2), 0x24943634)


if __name__ == "__main__":
    unittest.main()

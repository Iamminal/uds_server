# DS Diagnostics Server — ISO 14229 over virtual CAN

A compact but complete learning implementation of a stateful UDS ECU. The C++17 server runs on Linux SocketCAN, transports UDS messages with ISO-TP, and exposes diagnostic sessions, seed/key security access, DIDs, routine control, and DTC services. A dependency-free Python client and a Qt CAN/UDS monitor exercise the same raw frames on `vcan0`.

> This is a diagnostic/security demonstrator, not production ECU software. The seed/key algorithm is intentionally documented and reversible so that protocol behavior can be tested. A production design should use an OEM-approved cryptographic challenge-response mechanism, protected key material, secure boot, authenticated update paths, audit logging, and ISO 21434 threat/risk analysis.

## Implemented services

| SID | Service | Highlights |
|---:|---|---|
| `0x10` | DiagnosticSessionControl | Default, programming, extended; P2/P2* response; five-second S3 fallback |
| `0x11` | ECUReset | Hard, key-off/on, and soft reset simulation |
| `0x14` | ClearDiagnosticInformation | Clear all or matching three-byte DTC group |
| `0x19` | ReadDTCInformation | Sub-functions `01`, `02`, `04`, `06`, and `0A` |
| `0x22` | ReadDataByIdentifier | One or multiple DIDs; multi-frame responses |
| `0x27` | SecurityAccess | Two levels, sequence enforcement, three-attempt lockout, delay timer |
| `0x28` | CommunicationControl | Validates and acknowledges control type |
| `0x2E` | WriteDataByIdentifier | Session- and security-protected workshop fingerprint |
| `0x31` | RoutineControl | Start, stop, poll results; self-test and protected programming routines |
| `0x3E` | TesterPresent | Refreshes the S3 session timer; suppress-positive-response supported |

The ISO-TP layer supports single, first, consecutive, and flow-control frames, 12-bit payload lengths up to 4095 bytes, sequence validation, block size, STmin, and receive timeout recovery. Physical IDs default to `0x7E0`/`0x7E8`; functional requests are accepted on `0x7DF`.

## Repository layout

```text
uds_server/
├── server/include/uds/       public C++ headers
├── server/src/               server and protocol implementation
├── client/uds_client.py      raw SocketCAN/ISO-TP CLI and shell
├── monitor/                  Qt Widgets + Qt SerialBus monitor/client
├── cmake/version.h.in        generated version header
├── scripts/setup_vcan.sh     idempotent vcan setup helper
├── tests/uds_core_tests.cpp  manager/message unit tests
└── CMakeLists.txt
```

## Prerequisites

- Linux with SocketCAN and the `vcan` kernel module
- CMake 3.16+ and a C++17 compiler
- Python 3.9+ for the test client (no third-party packages)
- Optional monitor: Qt 5.15 or Qt 6 with Widgets and SerialBus/SocketCAN plugin
- Optional command-line observation: `can-utils`

On Debian/Ubuntu:

```bash
sudo apt update
sudo apt install build-essential cmake can-utils python3 \
  qtbase5-dev qt5-qmake libqt5serialbus5-dev libqt5serialbus5-plugins
```

## Configure virtual CAN

```bash
cd uds_server
bash scripts/setup_vcan.sh vcan0
```

The equivalent commands are:

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan   # only once
sudo ip link set up vcan0
```

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Start the ECU in one terminal:

```bash
./build/uds_server --interface vcan0
```

Useful options are `--request-id`, `--response-id`, `--functional-id`, and `--quiet`. CAN IDs accept decimal or `0x` notation.

## Python client

Run the end-to-end scenario (extended session, unlock, multi-frame DID read, protected write, routine, and DTC read):

```bash
python3 client/uds_client.py -i vcan0 demo
```

Individual operations:

```bash
python3 client/uds_client.py session 3
python3 client/uds_client.py unlock 1
python3 client/uds_client.py read 0xF190 0xF195
python3 client/uds_client.py write 0xF1A0 CODEX-WORKSHOP
python3 client/uds_client.py routine 1 0xFF00 CA FE
python3 client/uds_client.py dtc --mask 0xFF
python3 client/uds_client.py raw 0x19 0A
python3 client/uds_client.py shell
```

UDS sessions and security state belong to the ECU, not to the client process. You may issue session and unlock commands in separate invocations as long as the server's five-second S3 timer does not expire; the interactive shell is more convenient for longer sequences. Add `-v` to print every CAN and UDS frame.

The demo seed/key function is shared by C++ and Python:

1. rotate the 32-bit seed left by `level × 5` bits;
2. XOR with `0xA5C31F27 + level × 0x01020304`;
3. add `0x13579BDF`, retaining 32 bits.

Three invalid keys activate a ten-second delay. Requesting a key without a preceding seed returns NRC `0x24`.

## Qt CAN monitor/client

Build it through CMake:

```bash
cmake -S . -B build-qt -DBUILD_QT_MONITOR=ON
cmake --build build-qt -j
./build-qt/can_monitor
```

Or use qmake:

```bash
cd monitor
qmake CanMonitor.pro
make -j
./CanMonitor
```

The monitor shows timestamps, direction, CAN IDs, raw bytes, ISO-TP interpretation, reassembled UDS messages, search/filtering, and traffic statistics. Its request field is an active ISO-TP client; enter bytes such as `22 F1 90`, or use the quick-action buttons.

## Observe and inject frames

Watch all traffic:

```bash
candump -tz vcan0
```

Inject a single-frame VIN request manually (`03` bytes of UDS data):

```bash
cansend vcan0 7E0#0322F19000000000
```

The VIN is longer than one CAN frame, so the ECU sends a first frame on `0x7E8`. A real ISO-TP client must answer with flow control on `0x7E0`; use the included Python or Qt client to see the complete response.

## Built-in data

| DID | Meaning | Access |
|---:|---|---|
| `F190` | 17-byte VIN | Read, all sessions |
| `F195` | Software version | Read, all sessions |
| `F18C` | ECU serial number | Read, all sessions |
| `F187` | Spare part number | Read, all sessions |
| `F1A0` | Workshop fingerprint | Read/write, non-default session + security level 1 |
| `0100` | Supply voltage in mV | Read, all sessions |

Routine `FF00` is an unprotected self-test. Routines `0202` and `0203` model protected programming dependency/erase operations. Routines report in-progress status `01` and complete after 100 ms with status `00`.

## State-machine notes

- Entering any session resets the security state. ECU reset and S3 expiry return to default and lock security.
- Default session permits session control, reset, DID reads, DTC reads, and TesterPresent. Mutating services require a non-default session.
- The server sends negative responses for malformed order, size, access, and state transitions using ISO 14229 NRCs.
- Suppress-positive-response (`0x80` in a supported sub-function byte) suppresses only successful responses; errors are still returned.
- Transport errors reset in-progress ISO-TP state without terminating the server.

## Limitations

The project uses classic 8-byte CAN frames and normal addressing. It does not implement CAN FD, extended/mixed ISO-TP addressing, UDS download/upload/transfer services, persistent NVRAM, or real ECU reset/flash operations. Those are deliberate boundaries for a safe, inspectable virtual ECU.

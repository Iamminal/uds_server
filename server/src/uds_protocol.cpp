#include "uds/uds_protocol.h"

#include "uds/uds_exceptions.h"

#include <algorithm>
#include <chrono>
#include <thread>

namespace uds {

UdsProtocol::UdsProtocol(CanSocket& socket, std::uint32_t transmitId)
    : socket_(socket), transmitId_(transmitId) {}

void UdsProtocol::reset() {
    rx_.reset();
    tx_.reset();
}

std::optional<Bytes> UdsProtocol::ingest(const CanFrame& frame) {
    if (frame.data.empty()) return std::nullopt;
    const std::uint8_t type = frame.data[0] >> 4U;

    if (type == 0x3) {
        handleFlowControl(frame.data);
        return std::nullopt;
    }

    if (rx_ && std::chrono::steady_clock::now() - rx_->lastFrame > std::chrono::seconds(1)) rx_.reset();

    if (type == 0x0) {
        const std::size_t length = frame.data[0] & 0x0FU;
        if (length == 0 || length > 7 || frame.data.size() < length + 1) {
            throw TransportError("malformed ISO-TP single frame");
        }
        rx_.reset();
        return Bytes(frame.data.begin() + 1, frame.data.begin() + 1 + static_cast<std::ptrdiff_t>(length));
    }

    if (type == 0x1) {
        if (frame.data.size() < 8) throw TransportError("malformed ISO-TP first frame");
        const std::size_t length = (static_cast<std::size_t>(frame.data[0] & 0x0FU) << 8U) | frame.data[1];
        if (length <= 7 || length > kMaxIsoTpPayload) throw TransportError("invalid ISO-TP payload length");
        ReceiveState state;
        state.expectedLength = length;
        state.buffer.assign(frame.data.begin() + 2, frame.data.end());
        state.lastFrame = std::chrono::steady_clock::now();
        rx_ = std::move(state);
        sendFlowControl(0, 0, 0);  // continue, unlimited block, no separation delay
        return std::nullopt;
    }

    if (type == 0x2) {
        if (!rx_) throw TransportError("ISO-TP consecutive frame without first frame");
        const std::uint8_t sequence = frame.data[0] & 0x0FU;
        if (sequence != rx_->nextSequence) {
            rx_.reset();
            throw TransportError("ISO-TP sequence number mismatch");
        }
        rx_->nextSequence = static_cast<std::uint8_t>((rx_->nextSequence + 1U) & 0x0FU);
        rx_->buffer.insert(rx_->buffer.end(), frame.data.begin() + 1, frame.data.end());
        rx_->lastFrame = std::chrono::steady_clock::now();
        if (rx_->buffer.size() >= rx_->expectedLength) {
            rx_->buffer.resize(rx_->expectedLength);
            Bytes complete = std::move(rx_->buffer);
            rx_.reset();
            return complete;
        }
        return std::nullopt;
    }

    throw TransportError("unsupported ISO-TP protocol control information");
}

void UdsProtocol::send(const Bytes& payload) {
    if (payload.empty() || payload.size() > kMaxIsoTpPayload) throw TransportError("invalid ISO-TP payload size");
    if (tx_) throw TransportError("another ISO-TP response is still in progress");

    if (payload.size() <= 7) {
        Bytes frame{static_cast<std::uint8_t>(payload.size())};
        frame.insert(frame.end(), payload.begin(), payload.end());
        frame.resize(8, 0x00);
        sendFrame(std::move(frame));
        return;
    }

    Bytes first{
        static_cast<std::uint8_t>(0x10U | ((payload.size() >> 8U) & 0x0FU)),
        static_cast<std::uint8_t>(payload.size() & 0xFFU)
    };
    first.insert(first.end(), payload.begin(), payload.begin() + 6);
    sendFrame(std::move(first));

    TransmitState state;
    state.payload = payload;
    state.offset = 6;
    state.waitingForFlowControl = true;
    tx_ = std::move(state);
}

void UdsProtocol::sendFlowControl(std::uint8_t status, std::uint8_t blockSize, std::uint8_t stMin) {
    sendFrame({static_cast<std::uint8_t>(0x30U | (status & 0x0FU)), blockSize, stMin, 0, 0, 0, 0, 0});
}

void UdsProtocol::handleFlowControl(const Bytes& data) {
    if (!tx_ || data.size() < 3) return;
    const std::uint8_t status = data[0] & 0x0FU;
    if (status == 0x0) {
        tx_->blockSize = data[1];
        tx_->sentInBlock = 0;
        tx_->separationTime = data[2];
        tx_->waitingForFlowControl = false;
        continueTransmission();
    } else if (status == 0x1) {
        tx_->waitingForFlowControl = true;
    } else {
        tx_.reset();
        throw TransportError("ISO-TP receiver aborted transmission");
    }
}

void UdsProtocol::continueTransmission() {
    while (tx_ && tx_->offset < tx_->payload.size()) {
        if (tx_->blockSize != 0 && tx_->sentInBlock >= tx_->blockSize) {
            tx_->waitingForFlowControl = true;
            return;
        }

        Bytes frame{static_cast<std::uint8_t>(0x20U | tx_->nextSequence)};
        const std::size_t count = std::min<std::size_t>(7, tx_->payload.size() - tx_->offset);
        frame.insert(frame.end(), tx_->payload.begin() + static_cast<std::ptrdiff_t>(tx_->offset),
                     tx_->payload.begin() + static_cast<std::ptrdiff_t>(tx_->offset + count));
        frame.resize(8, 0x00);
        sendFrame(std::move(frame));

        tx_->offset += count;
        tx_->nextSequence = static_cast<std::uint8_t>((tx_->nextSequence + 1U) & 0x0FU);
        ++tx_->sentInBlock;

        if (tx_->offset < tx_->payload.size()) {
            if (tx_->separationTime <= 0x7F) {
                std::this_thread::sleep_for(std::chrono::milliseconds(tx_->separationTime));
            } else if (tx_->separationTime >= 0xF1 && tx_->separationTime <= 0xF9) {
                std::this_thread::sleep_for(std::chrono::microseconds((tx_->separationTime - 0xF0) * 100));
            }
        }
    }
    if (tx_ && tx_->offset >= tx_->payload.size()) tx_.reset();
}

void UdsProtocol::sendFrame(Bytes data) {
    if (data.size() > 8) throw TransportError("internal ISO-TP frame overflow");
    data.resize(8, 0x00);
    socket_.send(CanFrame{transmitId_, transmitId_ > 0x7FFU, std::move(data)});
}

}  // namespace uds

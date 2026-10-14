#pragma once

#include "uds/can_socket.h"
#include "uds/uds_constants.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace uds {

class UdsProtocol {
public:
    UdsProtocol(CanSocket& socket, std::uint32_t transmitId);

    std::optional<Bytes> ingest(const CanFrame& frame);
    void send(const Bytes& payload);
    void reset();

private:
    struct ReceiveState {
        Bytes buffer;
        std::size_t expectedLength{0};
        std::uint8_t nextSequence{1};
        std::chrono::steady_clock::time_point lastFrame{};
    };
    struct TransmitState {
        Bytes payload;
        std::size_t offset{0};
        std::uint8_t nextSequence{1};
        std::uint8_t blockSize{0};
        std::uint8_t sentInBlock{0};
        std::uint8_t separationTime{0};
        bool waitingForFlowControl{false};
    };

    CanSocket& socket_;
    std::uint32_t transmitId_;
    std::optional<ReceiveState> rx_;
    std::optional<TransmitState> tx_;

    void sendFlowControl(std::uint8_t status, std::uint8_t blockSize, std::uint8_t stMin);
    void handleFlowControl(const Bytes& data);
    void continueTransmission();
    void sendFrame(Bytes data);
};

}  // namespace uds

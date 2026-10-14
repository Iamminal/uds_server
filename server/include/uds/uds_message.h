#pragma once

#include "uds/uds_constants.h"

#include <cstdint>
#include <string>

namespace uds {

class UdsMessage {
public:
    explicit UdsMessage(Bytes payload);

    [[nodiscard]] std::uint8_t serviceId() const;
    [[nodiscard]] const Bytes& payload() const noexcept { return payload_; }
    [[nodiscard]] Bytes data() const;
    [[nodiscard]] std::string toString() const;

    static UdsMessage positive(std::uint8_t requestSid, Bytes data = {});
    static UdsMessage negative(std::uint8_t requestSid, NegativeResponseCode code);

private:
    Bytes payload_;
};

}  // namespace uds

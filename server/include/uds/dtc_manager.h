#pragma once

#include "uds/uds_constants.h"

#include <cstdint>
#include <string>
#include <vector>

namespace uds {

struct DtcRecord {
    std::uint32_t code;
    std::uint8_t status;
    std::string description;
    Bytes snapshot;
    Bytes extendedData;
};

class DtcManager {
public:
    DtcManager();

    Bytes read(std::uint8_t subFunction, const Bytes& parameters) const;
    void clear(std::uint32_t group, Session session);
    void setStatus(std::uint32_t code, std::uint8_t status);
    [[nodiscard]] const std::vector<DtcRecord>& records() const noexcept { return records_; }

private:
    std::vector<DtcRecord> records_;
    std::uint8_t availabilityMask_{0xFF};
};

}  // namespace uds

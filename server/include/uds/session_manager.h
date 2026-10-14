#pragma once

#include "uds/uds_constants.h"

#include <chrono>
#include <cstdint>

namespace uds {

class SessionManager {
public:
    SessionManager();

    void changeSession(Session requested);
    void testerPresent();
    bool expireIfInactive();
    void reset();

    [[nodiscard]] Session current() const noexcept { return current_; }
    [[nodiscard]] bool serviceAllowed(std::uint8_t sid) const;
    [[nodiscard]] std::uint16_t p2ServerMaxMs() const noexcept { return p2ServerMaxMs_; }
    [[nodiscard]] std::uint16_t p2StarServerMax10Ms() const noexcept { return p2StarServerMax10Ms_; }

private:
    using Clock = std::chrono::steady_clock;
    Session current_{Session::Default};
    Clock::time_point lastActivity_;
    std::chrono::milliseconds s3Timeout_{5000};
    std::uint16_t p2ServerMaxMs_{50};
    std::uint16_t p2StarServerMax10Ms_{500};
};

}  // namespace uds

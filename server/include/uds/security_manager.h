#pragma once

#include "uds/uds_constants.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <random>

namespace uds {

class SecurityManager {
public:
    SecurityManager();

    Bytes requestSeed(std::uint8_t level);
    void submitKey(std::uint8_t level, const Bytes& key);
    void reset();

    [[nodiscard]] bool isUnlocked(std::uint8_t level = 1) const noexcept;
    [[nodiscard]] static std::uint32_t calculateKey(std::uint32_t seed, std::uint8_t level) noexcept;

private:
    struct LevelState {
        std::optional<std::uint32_t> seed;
        bool unlocked{false};
        unsigned failedAttempts{0};
        std::chrono::steady_clock::time_point lockedUntil{};
    };

    static constexpr unsigned kMaximumAttempts = 3;
    static constexpr auto kAttemptDelay = std::chrono::seconds(10);
    std::array<LevelState, 2> levels_{};
    std::mt19937 random_;

    LevelState& stateFor(std::uint8_t level);
    const LevelState& stateFor(std::uint8_t level) const;
};

}  // namespace uds

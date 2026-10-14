#include "uds/security_manager.h"

#include "uds/uds_exceptions.h"

#include <algorithm>
#include <limits>

namespace uds {
namespace {

std::uint32_t readU32(const Bytes& value) {
    return (static_cast<std::uint32_t>(value[0]) << 24U) |
           (static_cast<std::uint32_t>(value[1]) << 16U) |
           (static_cast<std::uint32_t>(value[2]) << 8U) |
           static_cast<std::uint32_t>(value[3]);
}

Bytes writeU32(std::uint32_t value) {
    return {static_cast<std::uint8_t>(value >> 24U), static_cast<std::uint8_t>(value >> 16U),
            static_cast<std::uint8_t>(value >> 8U), static_cast<std::uint8_t>(value)};
}

}  // namespace

SecurityManager::SecurityManager() : random_(std::random_device{}()) {}

SecurityManager::LevelState& SecurityManager::stateFor(std::uint8_t level) {
    if (level < 1 || level > levels_.size()) {
        throw UdsError(NegativeResponseCode::SubFunctionNotSupported, "unsupported security level");
    }
    return levels_[level - 1];
}

const SecurityManager::LevelState& SecurityManager::stateFor(std::uint8_t level) const {
    if (level < 1 || level > levels_.size()) {
        throw UdsError(NegativeResponseCode::SubFunctionNotSupported, "unsupported security level");
    }
    return levels_[level - 1];
}

Bytes SecurityManager::requestSeed(std::uint8_t level) {
    auto& state = stateFor(level);
    const auto now = std::chrono::steady_clock::now();
    if (now < state.lockedUntil) {
        throw UdsError(NegativeResponseCode::RequiredTimeDelayNotExpired, "security delay is active");
    }
    if (state.unlocked) return Bytes(4, 0x00);

    std::uniform_int_distribution<std::uint32_t> distribution(1, std::numeric_limits<std::uint32_t>::max());
    state.seed = distribution(random_);
    return writeU32(*state.seed);
}

void SecurityManager::submitKey(std::uint8_t level, const Bytes& key) {
    auto& state = stateFor(level);
    const auto now = std::chrono::steady_clock::now();
    if (now < state.lockedUntil) {
        throw UdsError(NegativeResponseCode::RequiredTimeDelayNotExpired, "security delay is active");
    }
    if (!state.seed) throw UdsError(NegativeResponseCode::RequestSequenceError, "request a seed before sending a key");
    if (key.size() != 4) {
        throw UdsError(NegativeResponseCode::IncorrectMessageLengthOrInvalidFormat, "security key must be four bytes");
    }

    const bool matches = readU32(key) == calculateKey(*state.seed, level);
    state.seed.reset();
    if (matches) {
        state.unlocked = true;
        state.failedAttempts = 0;
        return;
    }

    ++state.failedAttempts;
    if (state.failedAttempts >= kMaximumAttempts) {
        state.failedAttempts = 0;
        state.lockedUntil = now + kAttemptDelay;
        throw UdsError(NegativeResponseCode::ExceededNumberOfAttempts, "security attempts exceeded");
    }
    throw UdsError(NegativeResponseCode::InvalidKey, "invalid security key");
}

void SecurityManager::reset() {
    for (auto& state : levels_) {
        state.seed.reset();
        state.unlocked = false;
    }
}

bool SecurityManager::isUnlocked(std::uint8_t level) const noexcept {
    return level >= 1 && level <= levels_.size() && levels_[level - 1].unlocked;
}

std::uint32_t SecurityManager::calculateKey(std::uint32_t seed, std::uint8_t level) noexcept {
    const unsigned rotation = static_cast<unsigned>((level * 5U) & 31U);
    const std::uint32_t rotated = (seed << rotation) | (seed >> (32U - rotation));
    return (rotated ^ (0xA5C31F27U + static_cast<std::uint32_t>(level) * 0x1020304U)) + 0x13579BDFU;
}

}  // namespace uds

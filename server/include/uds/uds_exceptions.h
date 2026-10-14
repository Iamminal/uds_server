#pragma once

#include "uds/uds_constants.h"

#include <stdexcept>
#include <string>

namespace uds {

class UdsError : public std::runtime_error {
public:
    UdsError(NegativeResponseCode code, const std::string& message)
        : std::runtime_error(message), code_(code) {}

    [[nodiscard]] NegativeResponseCode code() const noexcept { return code_; }

private:
    NegativeResponseCode code_;
};

class TransportError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ConfigurationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

}  // namespace uds

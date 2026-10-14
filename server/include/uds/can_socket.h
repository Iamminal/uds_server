#pragma once

#include "uds/uds_constants.h"

#include <cstdint>
#include <optional>
#include <string>

namespace uds {

struct CanFrame {
    std::uint32_t id{0};
    bool extended{false};
    Bytes data;
};

class CanSocket {
public:
    explicit CanSocket(std::string interfaceName);
    ~CanSocket();

    CanSocket(const CanSocket&) = delete;
    CanSocket& operator=(const CanSocket&) = delete;
    CanSocket(CanSocket&& other) noexcept;
    CanSocket& operator=(CanSocket&& other) noexcept;

    void open();
    void close() noexcept;
    [[nodiscard]] bool isOpen() const noexcept { return fd_ >= 0; }
    [[nodiscard]] int nativeHandle() const noexcept { return fd_; }
    [[nodiscard]] const std::string& interfaceName() const noexcept { return interfaceName_; }

    void send(const CanFrame& frame) const;
    [[nodiscard]] std::optional<CanFrame> receive(int timeoutMs = -1) const;

private:
    std::string interfaceName_;
    int fd_{-1};
};

}  // namespace uds

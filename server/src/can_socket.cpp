#include "uds/can_socket.h"

#include "uds/uds_exceptions.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <utility>

#ifdef __linux__
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace uds {

CanSocket::CanSocket(std::string interfaceName) : interfaceName_(std::move(interfaceName)) {}

CanSocket::~CanSocket() { close(); }

CanSocket::CanSocket(CanSocket&& other) noexcept
    : interfaceName_(std::move(other.interfaceName_)), fd_(other.fd_) {
    other.fd_ = -1;
}

CanSocket& CanSocket::operator=(CanSocket&& other) noexcept {
    if (this != &other) {
        close();
        interfaceName_ = std::move(other.interfaceName_);
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

void CanSocket::open() {
#ifdef __linux__
    if (isOpen()) return;
    fd_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd_ < 0) throw TransportError("cannot create CAN socket: " + std::string(std::strerror(errno)));

    ifreq request{};
    std::strncpy(request.ifr_name, interfaceName_.c_str(), IFNAMSIZ - 1);
    if (::ioctl(fd_, SIOCGIFINDEX, &request) < 0) {
        const std::string error = std::strerror(errno);
        close();
        throw TransportError("cannot find CAN interface '" + interfaceName_ + "': " + error);
    }

    sockaddr_can address{};
    address.can_family = AF_CAN;
    address.can_ifindex = request.ifr_ifindex;
    if (::bind(fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        const std::string error = std::strerror(errno);
        close();
        throw TransportError("cannot bind to '" + interfaceName_ + "': " + error);
    }
#else
    throw TransportError("SocketCAN is available only on Linux");
#endif
}

void CanSocket::close() noexcept {
#ifdef __linux__
    if (fd_ >= 0) ::close(fd_);
#endif
    fd_ = -1;
}

void CanSocket::send(const CanFrame& frame) const {
#ifdef __linux__
    if (!isOpen()) throw TransportError("CAN socket is not open");
    if (frame.data.size() > CAN_MAX_DLEN) throw TransportError("classic CAN frame exceeds 8 bytes");

    can_frame raw{};
    raw.can_id = frame.id | (frame.extended ? CAN_EFF_FLAG : 0U);
    raw.can_dlc = static_cast<__u8>(frame.data.size());
    std::copy(frame.data.begin(), frame.data.end(), raw.data);
    if (::write(fd_, &raw, sizeof(raw)) != static_cast<ssize_t>(sizeof(raw))) {
        throw TransportError("failed to send CAN frame: " + std::string(std::strerror(errno)));
    }
#else
    (void)frame;
    throw TransportError("SocketCAN is available only on Linux");
#endif
}

std::optional<CanFrame> CanSocket::receive(int timeoutMs) const {
#ifdef __linux__
    if (!isOpen()) throw TransportError("CAN socket is not open");
    pollfd descriptor{fd_, POLLIN, 0};
    const int ready = ::poll(&descriptor, 1, timeoutMs);
    if (ready == 0) return std::nullopt;
    if (ready < 0) {
        if (errno == EINTR) return std::nullopt;
        throw TransportError("CAN poll failed: " + std::string(std::strerror(errno)));
    }

    can_frame raw{};
    const auto count = ::read(fd_, &raw, sizeof(raw));
    if (count < 0 && errno == EINTR) return std::nullopt;
    if (count != static_cast<ssize_t>(sizeof(raw))) throw TransportError("incomplete CAN frame received");
    if ((raw.can_id & CAN_ERR_FLAG) != 0U) return std::nullopt;

    CanFrame frame;
    frame.extended = (raw.can_id & CAN_EFF_FLAG) != 0U;
    frame.id = raw.can_id & (frame.extended ? CAN_EFF_MASK : CAN_SFF_MASK);
    frame.data.assign(raw.data, raw.data + std::min<std::size_t>(raw.can_dlc, CAN_MAX_DLEN));
    return frame;
#else
    (void)timeoutMs;
    throw TransportError("SocketCAN is available only on Linux");
#endif
}

}  // namespace uds

#include "uds/uds_message.h"

#include "uds/uds_exceptions.h"

#include <iomanip>
#include <sstream>
#include <utility>

namespace uds {

UdsMessage::UdsMessage(Bytes payload) : payload_(std::move(payload)) {
    if (payload_.empty()) {
        throw UdsError(NegativeResponseCode::IncorrectMessageLengthOrInvalidFormat,
                       "a UDS message must contain a service identifier");
    }
}

std::uint8_t UdsMessage::serviceId() const { return payload_.front(); }

Bytes UdsMessage::data() const { return Bytes(payload_.begin() + 1, payload_.end()); }

std::string UdsMessage::toString() const {
    std::ostringstream stream;
    stream << std::hex << std::uppercase << std::setfill('0');
    for (std::size_t i = 0; i < payload_.size(); ++i) {
        if (i != 0) stream << ' ';
        stream << std::setw(2) << static_cast<unsigned>(payload_[i]);
    }
    return stream.str();
}

UdsMessage UdsMessage::positive(std::uint8_t requestSid, Bytes data) {
    data.insert(data.begin(), static_cast<std::uint8_t>(requestSid + kPositiveResponseOffset));
    return UdsMessage(std::move(data));
}

UdsMessage UdsMessage::negative(std::uint8_t requestSid, NegativeResponseCode code) {
    return UdsMessage({kNegativeResponseSid, requestSid, static_cast<std::uint8_t>(code)});
}

}  // namespace uds

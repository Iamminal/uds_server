#include "uds/uds_server.h"

#include "uds/uds_exceptions.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
#include <utility>

namespace uds {
namespace {

std::uint16_t readU16(const Bytes& value, std::size_t offset) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(value[offset]) << 8U) | value[offset + 1]);
}

bool hasSuppressBit(std::uint8_t sid, const Bytes& data) {
    if (data.empty()) return false;
    switch (static_cast<Service>(sid)) {
        case Service::DiagnosticSessionControl:
        case Service::EcuReset:
        case Service::SecurityAccess:
        case Service::CommunicationControl:
        case Service::RoutineControl:
        case Service::ReadDtcInformation:
        case Service::TesterPresent:
            return (data[0] & 0x80U) != 0;
        default:
            return false;
    }
}

}  // namespace

UdsServer::UdsServer(ServerConfig config)
    : config_(std::move(config)), socket_(config_.interfaceName), protocol_(socket_, config_.responseId) {}

void UdsServer::run() {
    socket_.open();
    running_ = true;
    std::cout << "UDS server listening on " << config_.interfaceName << " (request 0x" << std::hex
              << std::uppercase << config_.requestId << ", response 0x" << config_.responseId << ")\n";

    while (running_) {
        if (sessions_.expireIfInactive()) {
            security_.reset();
            if (config_.verbose) std::cout << "S3 timeout: returned to default session\n";
        }

        const auto frame = socket_.receive(100);
        if (!frame) continue;
        if (frame->id != config_.requestId && frame->id != config_.functionalId) continue;

        try {
            const auto payload = protocol_.ingest(*frame);
            if (!payload) continue;

            UdsMessage request(*payload);
            if (config_.verbose) std::cout << "RX  " << request.toString() << '\n';
            const bool suppressPositive = hasSuppressBit(request.serviceId(), request.data());
            sessions_.testerPresent();

            UdsMessage response = [&]() {
                try {
                    return dispatch(request);
                } catch (const UdsError& error) {
                    if (config_.verbose) std::cerr << "UDS error: " << error.what() << '\n';
                    return UdsMessage::negative(request.serviceId(), error.code());
                } catch (const std::exception& error) {
                    std::cerr << "service failure: " << error.what() << '\n';
                    return UdsMessage::negative(request.serviceId(), NegativeResponseCode::GeneralReject);
                }
            }();

            if (suppressPositive && response.serviceId() != kNegativeResponseSid) continue;
            if (config_.verbose) std::cout << "TX  " << response.toString() << '\n';
            protocol_.send(response.payload());
        } catch (const TransportError& error) {
            std::cerr << "transport warning: " << error.what() << '\n';
            protocol_.reset();
        }
    }
    socket_.close();
}

UdsMessage UdsServer::dispatch(const UdsMessage& request) {
    const auto sid = request.serviceId();
    if (!sessions_.serviceAllowed(sid)) {
        throw UdsError(NegativeResponseCode::ServiceNotSupportedInActiveSession,
                       "service is unavailable in the active session");
    }

    switch (static_cast<Service>(sid)) {
        case Service::DiagnosticSessionControl: return sessionControl(request);
        case Service::EcuReset: return ecuReset(request);
        case Service::SecurityAccess: return securityAccess(request);
        case Service::ReadDataByIdentifier: return readData(request);
        case Service::WriteDataByIdentifier: return writeData(request);
        case Service::RoutineControl: return routineControl(request);
        case Service::ReadDtcInformation: return readDtc(request);
        case Service::ClearDiagnosticInformation: return clearDtc(request);
        case Service::TesterPresent: return testerPresent(request);
        case Service::CommunicationControl: {
            const Bytes input = request.data();
            if (input.size() != 2) {
                throw UdsError(NegativeResponseCode::IncorrectMessageLengthOrInvalidFormat,
                               "CommunicationControl requires control and communication type");
            }
            const auto controlType = static_cast<std::uint8_t>(input[0] & 0x7FU);
            if (controlType > 3) throw UdsError(NegativeResponseCode::SubFunctionNotSupported, "invalid control type");
            return UdsMessage::positive(request.serviceId(), {controlType});
        }
        default:
            throw UdsError(NegativeResponseCode::ServiceNotSupported, "service identifier is not implemented");
    }
}

UdsMessage UdsServer::sessionControl(const UdsMessage& request) {
    const Bytes input = request.data();
    if (input.size() != 1) {
        throw UdsError(NegativeResponseCode::IncorrectMessageLengthOrInvalidFormat, "session type required");
    }
    const auto subFunction = static_cast<std::uint8_t>(input[0] & 0x7FU);
    sessions_.changeSession(static_cast<Session>(subFunction));
    security_.reset();
    const auto p2 = sessions_.p2ServerMaxMs();
    const auto p2Star = sessions_.p2StarServerMax10Ms();
    return UdsMessage::positive(request.serviceId(),
        {subFunction, static_cast<std::uint8_t>(p2 >> 8U), static_cast<std::uint8_t>(p2),
         static_cast<std::uint8_t>(p2Star >> 8U), static_cast<std::uint8_t>(p2Star)});
}

UdsMessage UdsServer::ecuReset(const UdsMessage& request) {
    const Bytes input = request.data();
    if (input.size() != 1) {
        throw UdsError(NegativeResponseCode::IncorrectMessageLengthOrInvalidFormat, "reset type required");
    }
    const auto resetType = static_cast<std::uint8_t>(input[0] & 0x7FU);
    if (resetType < 1 || resetType > 3) throw UdsError(NegativeResponseCode::SubFunctionNotSupported, "unsupported reset type");
    sessions_.reset();
    security_.reset();
    return UdsMessage::positive(request.serviceId(), {resetType, 0x00});
}

UdsMessage UdsServer::securityAccess(const UdsMessage& request) {
    const Bytes input = request.data();
    if (input.empty()) {
        throw UdsError(NegativeResponseCode::IncorrectMessageLengthOrInvalidFormat, "security sub-function required");
    }
    const auto subFunction = static_cast<std::uint8_t>(input[0] & 0x7FU);
    if (subFunction == 0 || subFunction > 4) {
        throw UdsError(NegativeResponseCode::SubFunctionNotSupported, "unsupported security sub-function");
    }
    if ((subFunction & 1U) != 0U) {
        if (input.size() != 1) {
            throw UdsError(NegativeResponseCode::IncorrectMessageLengthOrInvalidFormat, "seed request has unexpected data");
        }
        const std::uint8_t level = static_cast<std::uint8_t>((subFunction + 1U) / 2U);
        Bytes output{subFunction};
        const Bytes seed = security_.requestSeed(level);
        output.insert(output.end(), seed.begin(), seed.end());
        return UdsMessage::positive(request.serviceId(), std::move(output));
    }

    const std::uint8_t level = static_cast<std::uint8_t>(subFunction / 2U);
    security_.submitKey(level, Bytes(input.begin() + 1, input.end()));
    return UdsMessage::positive(request.serviceId(), {subFunction});
}

UdsMessage UdsServer::readData(const UdsMessage& request) {
    const Bytes input = request.data();
    if (input.empty() || input.size() % 2 != 0) {
        throw UdsError(NegativeResponseCode::IncorrectMessageLengthOrInvalidFormat, "one or more DIDs are required");
    }
    Bytes output;
    for (std::size_t offset = 0; offset < input.size(); offset += 2) {
        const auto did = readU16(input, offset);
        const Bytes value = data_.read(did, sessions_.current(), security_.isUnlocked());
        output.push_back(input[offset]);
        output.push_back(input[offset + 1]);
        output.insert(output.end(), value.begin(), value.end());
    }
    return UdsMessage::positive(request.serviceId(), std::move(output));
}

UdsMessage UdsServer::writeData(const UdsMessage& request) {
    const Bytes input = request.data();
    if (input.size() < 3) {
        throw UdsError(NegativeResponseCode::IncorrectMessageLengthOrInvalidFormat, "DID and value are required");
    }
    const auto did = readU16(input, 0);
    data_.write(did, Bytes(input.begin() + 2, input.end()), sessions_.current(), security_.isUnlocked());
    return UdsMessage::positive(request.serviceId(), {input[0], input[1]});
}

UdsMessage UdsServer::routineControl(const UdsMessage& request) {
    const Bytes input = request.data();
    if (input.size() < 3) {
        throw UdsError(NegativeResponseCode::IncorrectMessageLengthOrInvalidFormat, "routine type and ID are required");
    }
    const auto typeValue = static_cast<std::uint8_t>(input[0] & 0x7FU);
    if (typeValue < 1 || typeValue > 3) throw UdsError(NegativeResponseCode::SubFunctionNotSupported, "invalid routine type");
    const auto routineId = readU16(input, 1);
    const Bytes result = routines_.control(static_cast<RoutineControlType>(typeValue), routineId,
                                           Bytes(input.begin() + 3, input.end()), sessions_.current(),
                                           security_.isUnlocked());
    Bytes output{typeValue, input[1], input[2]};
    output.insert(output.end(), result.begin(), result.end());
    return UdsMessage::positive(request.serviceId(), std::move(output));
}

UdsMessage UdsServer::readDtc(const UdsMessage& request) {
    const Bytes input = request.data();
    if (input.empty()) {
        throw UdsError(NegativeResponseCode::IncorrectMessageLengthOrInvalidFormat, "DTC sub-function required");
    }
    const auto subFunction = static_cast<std::uint8_t>(input[0] & 0x7FU);
    return UdsMessage::positive(request.serviceId(),
        dtcs_.read(subFunction, Bytes(input.begin() + 1, input.end())));
}

UdsMessage UdsServer::clearDtc(const UdsMessage& request) {
    const Bytes input = request.data();
    if (input.size() != 3) {
        throw UdsError(NegativeResponseCode::IncorrectMessageLengthOrInvalidFormat, "three-byte DTC group required");
    }
    const std::uint32_t group = (static_cast<std::uint32_t>(input[0]) << 16U) |
                                (static_cast<std::uint32_t>(input[1]) << 8U) | input[2];
    dtcs_.clear(group, sessions_.current());
    return UdsMessage::positive(request.serviceId());
}

UdsMessage UdsServer::testerPresent(const UdsMessage& request) {
    const Bytes input = request.data();
    if (input.size() != 1) {
        throw UdsError(NegativeResponseCode::IncorrectMessageLengthOrInvalidFormat, "TesterPresent requires one sub-function");
    }
    const auto subFunction = static_cast<std::uint8_t>(input[0] & 0x7FU);
    if (subFunction != 0) throw UdsError(NegativeResponseCode::SubFunctionNotSupported, "unsupported TesterPresent sub-function");
    sessions_.testerPresent();
    return UdsMessage::positive(request.serviceId(), {subFunction});
}

}  // namespace uds

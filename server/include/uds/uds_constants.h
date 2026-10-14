#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace uds {

using Bytes = std::vector<std::uint8_t>;

constexpr std::uint32_t kDefaultRequestId = 0x7E0;
constexpr std::uint32_t kDefaultResponseId = 0x7E8;
constexpr std::uint32_t kFunctionalRequestId = 0x7DF;
constexpr std::size_t kMaxIsoTpPayload = 4095;
constexpr std::uint8_t kPositiveResponseOffset = 0x40;
constexpr std::uint8_t kNegativeResponseSid = 0x7F;

enum class Service : std::uint8_t {
    DiagnosticSessionControl = 0x10,
    EcuReset = 0x11,
    ClearDiagnosticInformation = 0x14,
    ReadDtcInformation = 0x19,
    ReadDataByIdentifier = 0x22,
    SecurityAccess = 0x27,
    CommunicationControl = 0x28,
    WriteDataByIdentifier = 0x2E,
    RoutineControl = 0x31,
    TesterPresent = 0x3E,
};

enum class Session : std::uint8_t {
    Default = 0x01,
    Programming = 0x02,
    Extended = 0x03,
};

enum class NegativeResponseCode : std::uint8_t {
    GeneralReject = 0x10,
    ServiceNotSupported = 0x11,
    SubFunctionNotSupported = 0x12,
    IncorrectMessageLengthOrInvalidFormat = 0x13,
    ResponseTooLong = 0x14,
    BusyRepeatRequest = 0x21,
    ConditionsNotCorrect = 0x22,
    RequestSequenceError = 0x24,
    RequestOutOfRange = 0x31,
    SecurityAccessDenied = 0x33,
    InvalidKey = 0x35,
    ExceededNumberOfAttempts = 0x36,
    RequiredTimeDelayNotExpired = 0x37,
    UploadDownloadNotAccepted = 0x70,
    GeneralProgrammingFailure = 0x72,
    SubFunctionNotSupportedInActiveSession = 0x7E,
    ServiceNotSupportedInActiveSession = 0x7F,
    ResponsePending = 0x78,
};

enum class RoutineControlType : std::uint8_t {
    Start = 0x01,
    Stop = 0x02,
    RequestResults = 0x03,
};

enum class DtcStatus : std::uint8_t {
    TestFailed = 0x01,
    TestFailedThisOperationCycle = 0x02,
    Pending = 0x04,
    Confirmed = 0x08,
    TestNotCompletedSinceLastClear = 0x10,
    TestFailedSinceLastClear = 0x20,
    TestNotCompletedThisOperationCycle = 0x40,
    WarningIndicatorRequested = 0x80,
};

constexpr bool hasStatus(std::uint8_t value, DtcStatus bit) {
    return (value & static_cast<std::uint8_t>(bit)) != 0;
}

inline std::string hexByte(std::uint8_t value) {
    constexpr char digits[] = "0123456789ABCDEF";
    return {digits[(value >> 4) & 0x0F], digits[value & 0x0F]};
}

}  // namespace uds

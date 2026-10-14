#include "uds/dtc_manager.h"

#include "uds/uds_exceptions.h"

#include <algorithm>

namespace uds {
namespace {

void appendDtc(Bytes& output, const DtcRecord& record) {
    output.push_back(static_cast<std::uint8_t>(record.code >> 16U));
    output.push_back(static_cast<std::uint8_t>(record.code >> 8U));
    output.push_back(static_cast<std::uint8_t>(record.code));
    output.push_back(record.status);
}

std::uint32_t parseDtc(const Bytes& data, std::size_t offset) {
    return (static_cast<std::uint32_t>(data[offset]) << 16U) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8U) |
           data[offset + 2];
}

}  // namespace

DtcManager::DtcManager() {
    records_.push_back({0x123456, 0x2F, "Intake temperature signal implausible", {0x01, 0x5A, 0x30, 0xD4}, {0x01, 0x03}});
    records_.push_back({0xABCDEF, 0x09, "Virtual CAN communication timeout", {0x01, 0x40, 0x2F, 0x10}, {0x01, 0x07}});
    records_.push_back({0xC00100, 0x04, "Supply voltage below threshold", {0x01, 0x20, 0x23, 0x28}, {0x01, 0x01}});
}

Bytes DtcManager::read(std::uint8_t subFunction, const Bytes& parameters) const {
    switch (subFunction) {
        case 0x01: {  // reportNumberOfDTCByStatusMask
            if (parameters.size() != 1) throw UdsError(NegativeResponseCode::IncorrectMessageLengthOrInvalidFormat, "status mask required");
            const auto count = std::count_if(records_.begin(), records_.end(), [&](const auto& record) {
                return (record.status & parameters[0]) != 0;
            });
            return {subFunction, availabilityMask_, 0x01, static_cast<std::uint8_t>(count >> 8U), static_cast<std::uint8_t>(count)};
        }
        case 0x02: {  // reportDTCByStatusMask
            if (parameters.size() != 1) throw UdsError(NegativeResponseCode::IncorrectMessageLengthOrInvalidFormat, "status mask required");
            Bytes result{subFunction, availabilityMask_};
            for (const auto& record : records_) if ((record.status & parameters[0]) != 0) appendDtc(result, record);
            return result;
        }
        case 0x04: {  // reportDTCSnapshotRecordByDTCNumber
            if (parameters.size() != 4) throw UdsError(NegativeResponseCode::IncorrectMessageLengthOrInvalidFormat, "DTC and snapshot record required");
            const auto code = parseDtc(parameters, 0);
            const auto found = std::find_if(records_.begin(), records_.end(), [&](const auto& value) { return value.code == code; });
            if (found == records_.end()) throw UdsError(NegativeResponseCode::RequestOutOfRange, "DTC not found");
            Bytes result{subFunction, parameters[0], parameters[1], parameters[2], found->status, parameters[3]};
            result.insert(result.end(), found->snapshot.begin(), found->snapshot.end());
            return result;
        }
        case 0x06: {  // reportDTCExtDataRecordByDTCNumber
            if (parameters.size() != 4) throw UdsError(NegativeResponseCode::IncorrectMessageLengthOrInvalidFormat, "DTC and extended record required");
            const auto code = parseDtc(parameters, 0);
            const auto found = std::find_if(records_.begin(), records_.end(), [&](const auto& value) { return value.code == code; });
            if (found == records_.end()) throw UdsError(NegativeResponseCode::RequestOutOfRange, "DTC not found");
            Bytes result{subFunction, parameters[0], parameters[1], parameters[2], found->status, parameters[3]};
            result.insert(result.end(), found->extendedData.begin(), found->extendedData.end());
            return result;
        }
        case 0x0A: {  // reportSupportedDTC
            if (!parameters.empty()) throw UdsError(NegativeResponseCode::IncorrectMessageLengthOrInvalidFormat, "unexpected parameters");
            Bytes result{subFunction, availabilityMask_};
            for (const auto& record : records_) appendDtc(result, record);
            return result;
        }
        default:
            throw UdsError(NegativeResponseCode::SubFunctionNotSupported, "unsupported ReadDTCInformation sub-function");
    }
}

void DtcManager::clear(std::uint32_t group, Session session) {
    if (session == Session::Default) {
        throw UdsError(NegativeResponseCode::ServiceNotSupportedInActiveSession, "DTC clearing requires extended session");
    }
    if (group == 0xFFFFFF) {
        for (auto& record : records_) record.status = 0;
        return;
    }
    bool matched = false;
    for (auto& record : records_) {
        if ((record.code & 0xFFFF00U) == (group & 0xFFFF00U) || record.code == group) {
            record.status = 0;
            matched = true;
        }
    }
    if (!matched) throw UdsError(NegativeResponseCode::RequestOutOfRange, "DTC group not found");
}

void DtcManager::setStatus(std::uint32_t code, std::uint8_t status) {
    const auto found = std::find_if(records_.begin(), records_.end(), [&](const auto& value) { return value.code == code; });
    if (found == records_.end()) throw UdsError(NegativeResponseCode::RequestOutOfRange, "DTC not found");
    found->status = status;
}

}  // namespace uds

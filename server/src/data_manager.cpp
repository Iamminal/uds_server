#include "uds/data_manager.h"

#include "uds/uds_exceptions.h"

#include <utility>

namespace uds {
namespace {
Bytes ascii(const std::string& text) { return Bytes(text.begin(), text.end()); }
}

DataManager::DataManager() {
    dids_.emplace(0xF190, DidEntry{"Vehicle identification number", ascii("WVWZZZ1JZXW000001"), false, false, false, 17, 17});
    dids_.emplace(0xF195, DidEntry{"Software version", ascii("UDS-DEMO-1.0.0"), false, false, false, 1, 32});
    dids_.emplace(0xF18C, DidEntry{"ECU serial number", ascii("ECU-2026-0001"), false, false, false, 1, 32});
    dids_.emplace(0xF187, DidEntry{"Spare part number", ascii("DS-14229-VCAN"), false, false, false, 1, 32});
    dids_.emplace(0xF1A0, DidEntry{"Workshop fingerprint", ascii("UNPROGRAMMED"), true, true, true, 1, 32});
    dids_.emplace(0x0100, DidEntry{"Supply voltage (millivolts)", {0x30, 0xD4}, false, false, false, 2, 2});
}

const DataManager::DidEntry& DataManager::find(std::uint16_t did) const {
    const auto entry = dids_.find(did);
    if (entry == dids_.end()) throw UdsError(NegativeResponseCode::RequestOutOfRange, "unknown data identifier");
    return entry->second;
}

DataManager::DidEntry& DataManager::find(std::uint16_t did) {
    const auto entry = dids_.find(did);
    if (entry == dids_.end()) throw UdsError(NegativeResponseCode::RequestOutOfRange, "unknown data identifier");
    return entry->second;
}

Bytes DataManager::read(std::uint16_t did, Session session, bool securityUnlocked) const {
    const auto& entry = find(did);
    if (entry.extendedRequired && session == Session::Default) {
        throw UdsError(NegativeResponseCode::RequestOutOfRange, "DID is unavailable in the default session");
    }
    if (entry.securityRequired && !securityUnlocked) {
        throw UdsError(NegativeResponseCode::SecurityAccessDenied, "DID requires security access");
    }
    return entry.value;
}

void DataManager::write(std::uint16_t did, const Bytes& value, Session session, bool securityUnlocked) {
    auto& entry = find(did);
    if (!entry.writable) throw UdsError(NegativeResponseCode::RequestOutOfRange, "DID is read-only");
    if (entry.extendedRequired && session == Session::Default) {
        throw UdsError(NegativeResponseCode::SubFunctionNotSupportedInActiveSession, "extended session required");
    }
    if (entry.securityRequired && !securityUnlocked) {
        throw UdsError(NegativeResponseCode::SecurityAccessDenied, "security access required");
    }
    if (value.size() < entry.minLength || value.size() > entry.maxLength) {
        throw UdsError(NegativeResponseCode::IncorrectMessageLengthOrInvalidFormat, "DID value has invalid length");
    }
    entry.value = value;
}

std::string DataManager::describe(std::uint16_t did) const { return find(did).name; }

}  // namespace uds

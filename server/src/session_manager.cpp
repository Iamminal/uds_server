#include "uds/session_manager.h"

#include "uds/uds_exceptions.h"

namespace uds {

SessionManager::SessionManager() : lastActivity_(Clock::now()) {}

void SessionManager::changeSession(Session requested) {
    switch (requested) {
        case Session::Default:
        case Session::Programming:
        case Session::Extended:
            current_ = requested;
            lastActivity_ = Clock::now();
            return;
    }
    throw UdsError(NegativeResponseCode::SubFunctionNotSupported, "unsupported diagnostic session");
}

void SessionManager::testerPresent() { lastActivity_ = Clock::now(); }

bool SessionManager::expireIfInactive() {
    if (current_ != Session::Default && Clock::now() - lastActivity_ > s3Timeout_) {
        reset();
        return true;
    }
    return false;
}

void SessionManager::reset() {
    current_ = Session::Default;
    lastActivity_ = Clock::now();
}

bool SessionManager::serviceAllowed(std::uint8_t sid) const {
    if (current_ != Session::Default) return true;
    switch (static_cast<Service>(sid)) {
        case Service::DiagnosticSessionControl:
        case Service::EcuReset:
        case Service::ReadDataByIdentifier:
        case Service::ReadDtcInformation:
        case Service::TesterPresent:
            return true;
        default:
            return false;
    }
}

}  // namespace uds

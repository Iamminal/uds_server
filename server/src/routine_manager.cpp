#include "uds/routine_manager.h"

#include "uds/uds_exceptions.h"

#include <numeric>

namespace uds {

RoutineManager::RoutineManager() {
    routines_.emplace(0x0202, Routine{"Check programming dependencies", true});
    routines_.emplace(0x0203, Routine{"Erase emulated memory", true});
    routines_.emplace(0xFF00, Routine{"System self-test", false});
}

Bytes RoutineManager::control(RoutineControlType type, std::uint16_t routineId, const Bytes& optionRecord,
                              Session session, bool securityUnlocked) {
    const auto found = routines_.find(routineId);
    if (found == routines_.end()) throw UdsError(NegativeResponseCode::RequestOutOfRange, "unknown routine identifier");
    auto& routine = found->second;

    if (session == Session::Default) {
        throw UdsError(NegativeResponseCode::SubFunctionNotSupportedInActiveSession, "routine requires a non-default session");
    }
    if (routine.securityRequired && !securityUnlocked) {
        throw UdsError(NegativeResponseCode::SecurityAccessDenied, "routine requires security access");
    }

    switch (type) {
        case RoutineControlType::Start: {
            if (routine.running) throw UdsError(NegativeResponseCode::RequestSequenceError, "routine is already running");
            routine.running = true;
            routine.started = std::chrono::steady_clock::now();
            const std::uint8_t checksum = std::accumulate(optionRecord.begin(), optionRecord.end(), std::uint8_t{0},
                [](std::uint8_t sum, std::uint8_t value) { return static_cast<std::uint8_t>(sum + value); });
            routine.lastResult = {0x01, checksum};  // in progress, option checksum
            return routine.lastResult;
        }
        case RoutineControlType::Stop:
            if (!routine.running) throw UdsError(NegativeResponseCode::RequestSequenceError, "routine is not running");
            routine.running = false;
            routine.lastResult[0] = 0x00;
            return routine.lastResult;
        case RoutineControlType::RequestResults:
            // Demo routines complete after 100 ms; this makes polling observable.
            if (routine.running && std::chrono::steady_clock::now() - routine.started > std::chrono::milliseconds(100)) {
                routine.running = false;
                routine.lastResult[0] = 0x00;
            }
            return routine.lastResult;
    }
    throw UdsError(NegativeResponseCode::SubFunctionNotSupported, "unsupported routine-control type");
}

}  // namespace uds

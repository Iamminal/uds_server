#pragma once

#include "uds/uds_constants.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <string>

namespace uds {

class RoutineManager {
public:
    RoutineManager();

    Bytes control(RoutineControlType type, std::uint16_t routineId, const Bytes& optionRecord,
                  Session session, bool securityUnlocked);

private:
    struct Routine {
        std::string name;
        bool securityRequired{false};
        bool running{false};
        Bytes lastResult{0x00};
        std::chrono::steady_clock::time_point started{};
    };

    std::map<std::uint16_t, Routine> routines_;
};

}  // namespace uds

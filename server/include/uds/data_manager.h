#pragma once

#include "uds/uds_constants.h"

#include <cstdint>
#include <map>
#include <string>

namespace uds {

class DataManager {
public:
    DataManager();

    Bytes read(std::uint16_t did, Session session, bool securityUnlocked) const;
    void write(std::uint16_t did, const Bytes& value, Session session, bool securityUnlocked);
    [[nodiscard]] std::string describe(std::uint16_t did) const;

private:
    struct DidEntry {
        std::string name;
        Bytes value;
        bool writable{false};
        bool extendedRequired{false};
        bool securityRequired{false};
        std::size_t minLength{0};
        std::size_t maxLength{0};
    };

    std::map<std::uint16_t, DidEntry> dids_;
    const DidEntry& find(std::uint16_t did) const;
    DidEntry& find(std::uint16_t did);
};

}  // namespace uds

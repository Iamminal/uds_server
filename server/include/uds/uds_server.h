#pragma once

#include "uds/can_socket.h"
#include "uds/data_manager.h"
#include "uds/dtc_manager.h"
#include "uds/routine_manager.h"
#include "uds/security_manager.h"
#include "uds/session_manager.h"
#include "uds/uds_message.h"
#include "uds/uds_protocol.h"

#include <atomic>
#include <cstdint>
#include <string>

namespace uds {

struct ServerConfig {
    std::string interfaceName{"vcan0"};
    std::uint32_t requestId{kDefaultRequestId};
    std::uint32_t responseId{kDefaultResponseId};
    std::uint32_t functionalId{kFunctionalRequestId};
    bool verbose{true};
};

class UdsServer {
public:
    explicit UdsServer(ServerConfig config = {});

    void run();
    void stop() noexcept { running_ = false; }

private:
    ServerConfig config_;
    CanSocket socket_;
    UdsProtocol protocol_;
    SessionManager sessions_;
    SecurityManager security_;
    DataManager data_;
    RoutineManager routines_;
    DtcManager dtcs_;
    std::atomic_bool running_{false};

    UdsMessage dispatch(const UdsMessage& request);
    UdsMessage sessionControl(const UdsMessage& request);
    UdsMessage ecuReset(const UdsMessage& request);
    UdsMessage securityAccess(const UdsMessage& request);
    UdsMessage readData(const UdsMessage& request);
    UdsMessage writeData(const UdsMessage& request);
    UdsMessage routineControl(const UdsMessage& request);
    UdsMessage readDtc(const UdsMessage& request);
    UdsMessage clearDtc(const UdsMessage& request);
    UdsMessage testerPresent(const UdsMessage& request);
};

}  // namespace uds

#include "uds/data_manager.h"
#include "uds/security_manager.h"
#include "uds/session_manager.h"
#include "uds/uds_exceptions.h"
#include "uds/uds_message.h"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

template <typename Function>
void expectNrc(uds::NegativeResponseCode code, Function&& function) {
    try {
        function();
        assert(false && "expected UdsError");
    } catch (const uds::UdsError& error) {
        assert(error.code() == code);
    }
}

}  // namespace

int main() {
    const auto positive = uds::UdsMessage::positive(0x22, {0xF1, 0x90});
    assert(positive.payload() == uds::Bytes({0x62, 0xF1, 0x90}));
    const auto negative = uds::UdsMessage::negative(0x2E, uds::NegativeResponseCode::SecurityAccessDenied);
    assert(negative.payload() == uds::Bytes({0x7F, 0x2E, 0x33}));

    assert(uds::SecurityManager::calculateKey(0x00000001U, 1) == 0xBA1CBDEAU);

    uds::SessionManager sessions;
    assert(sessions.current() == uds::Session::Default);
    assert(!sessions.serviceAllowed(0x2E));
    sessions.changeSession(uds::Session::Extended);
    assert(sessions.serviceAllowed(0x2E));

    uds::DataManager data;
    assert(data.read(0xF190, uds::Session::Default, false).size() == 17);
    expectNrc(uds::NegativeResponseCode::RequestOutOfRange,
              [&] { data.read(0xF1A0, uds::Session::Default, false); });
    expectNrc(uds::NegativeResponseCode::SecurityAccessDenied,
              [&] { data.read(0xF1A0, uds::Session::Extended, false); });
    data.write(0xF1A0, {'T', 'E', 'S', 'T'}, uds::Session::Extended, true);
    assert(data.read(0xF1A0, uds::Session::Extended, true) == uds::Bytes({'T', 'E', 'S', 'T'}));

    std::cout << "all uds_core tests passed\n";
    return 0;
}

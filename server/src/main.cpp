#include "uds/uds_server.h"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
uds::UdsServer* activeServer = nullptr;

void onSignal(int) {
    if (activeServer != nullptr) activeServer->stop();
}

std::uint32_t parseCanId(const std::string& value) {
    std::size_t consumed = 0;
    const auto parsed = std::stoul(value, &consumed, 0);
    if (consumed != value.size() || parsed > 0x1FFFFFFFU) throw std::invalid_argument("invalid CAN identifier: " + value);
    return static_cast<std::uint32_t>(parsed);
}

void usage(const char* executable) {
    std::cout << "Usage: " << executable << " [--interface vcan0] [--request-id 0x7E0] "
              << "[--response-id 0x7E8] [--functional-id 0x7DF] [--quiet]\n";
}
}  // namespace

int main(int argc, char** argv) {
    uds::ServerConfig config;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            auto argument = [&](const char* name) -> std::string {
                if (i + 1 >= argc) throw std::invalid_argument(std::string("missing value for ") + name);
                return argv[++i];
            };
            if (option == "--interface" || option == "-i") config.interfaceName = argument("--interface");
            else if (option == "--request-id") config.requestId = parseCanId(argument("--request-id"));
            else if (option == "--response-id") config.responseId = parseCanId(argument("--response-id"));
            else if (option == "--functional-id") config.functionalId = parseCanId(argument("--functional-id"));
            else if (option == "--quiet" || option == "-q") config.verbose = false;
            else if (option == "--help" || option == "-h") { usage(argv[0]); return EXIT_SUCCESS; }
            else throw std::invalid_argument("unknown option: " + option);
        }

        uds::UdsServer server(config);
        activeServer = &server;
        std::signal(SIGINT, onSignal);
        std::signal(SIGTERM, onSignal);
        server.run();
        activeServer = nullptr;
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << '\n';
        usage(argv[0]);
        return EXIT_FAILURE;
    }
}

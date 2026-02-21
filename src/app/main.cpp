#include "app/App.hpp"

#include <charconv>
#include <cstdint>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace {

void printUsage(std::ostream &out) {
    out << "Usage: sfml_2048 [options]\n"
        << "Options:\n"
        << "  --seed <uint32>    Start game with deterministic seed\n"
        << "  --fps <uint>       Set frame limit (0 disables limit)\n"
        << "  --vsync            Enable vertical sync (default)\n"
        << "  --no-vsync         Disable vertical sync\n"
        << "  --help             Show this help message\n";
}

bool parseUnsignedValue(const std::string_view text, unsigned int &value) {
    if (text.empty()) {
        return false;
    }

    unsigned long long parsed = 0;
    const auto *begin = text.data();
    const auto *end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed);
    if (ec != std::errc{} || ptr != end || parsed > std::numeric_limits<unsigned int>::max()) {
        return false;
    }

    value = static_cast<unsigned int>(parsed);
    return true;
}

bool parseUint32Value(const std::string_view text, std::uint32_t &value) {
    unsigned int parsed = 0;
    if (!parseUnsignedValue(text, parsed) || parsed > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }

    value = static_cast<std::uint32_t>(parsed);
    return true;
}

} // namespace

int main(int argc, char *argv[]) {
    app::RunConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];

        if (arg == "--help") {
            printUsage(std::cout);
            return 0;
        }

        if (arg == "--seed") {
            if (i + 1 >= argc) {
                std::cerr << "--seed requires a value\n";
                return 2;
            }

            std::uint32_t seed = 0;
            if (!parseUint32Value(argv[++i], seed)) {
                std::cerr << "invalid seed value\n";
                return 2;
            }
            config.seed = seed;
            continue;
        }

        if (arg == "--fps") {
            if (i + 1 >= argc) {
                std::cerr << "--fps requires a value\n";
                return 2;
            }

            unsigned int fps = 0;
            if (!parseUnsignedValue(argv[++i], fps)) {
                std::cerr << "invalid fps value\n";
                return 2;
            }
            config.frameLimit = fps;
            continue;
        }

        if (arg == "--vsync") {
            config.vSyncEnabled = true;
            continue;
        }

        if (arg == "--no-vsync") {
            config.vSyncEnabled = false;
            continue;
        }

        std::cerr << "Unknown argument: " << arg << "\n";
        printUsage(std::cerr);
        return 2;
    }

    return app::run(config);
}

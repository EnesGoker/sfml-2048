#include "app/App.hpp"

#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

int main(int argc, char *argv[]) {
    app::RunConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];

        if (arg == "--help") {
            std::cout << "Usage: sfml_2048 [--seed <uint32>]\n";
            return 0;
        }

        if (arg == "--seed") {
            if (i + 1 >= argc) {
                std::cerr << "--seed requires a value\n";
                return 2;
            }

            try {
                const unsigned long long parsed = std::stoull(argv[++i]);
                if (parsed > std::numeric_limits<std::uint32_t>::max()) {
                    std::cerr << "seed out of range for uint32\n";
                    return 2;
                }
                config.seed = static_cast<std::uint32_t>(parsed);
            } catch (const std::exception &) {
                std::cerr << "invalid seed value\n";
                return 2;
            }
            continue;
        }

        std::cerr << "Unknown argument: " << arg << "\n";
        std::cerr << "Usage: sfml_2048 [--seed <uint32>]\n";
        return 2;
    }

    return app::run(config);
}

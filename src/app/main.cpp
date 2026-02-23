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
    out << "Kullanim: sfml_2048 [secenekler]\n"
        << "Secenekler:\n"
        << "  --fps <uint>       Kare hizini ayarla (0 = sinirsiz)\n"
        << "  --vsync            Dikey senkronu ac (varsayilan)\n"
        << "  --no-vsync         Dikey senkronu kapat\n"
        << "  --help             Bu yardim mesajini goster\n";
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

} // namespace

int main(int argc, char *argv[]) {
    app::RunConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];

        if (arg == "--help") {
            printUsage(std::cout);
            return 0;
        }

        if (arg == "--fps") {
            if (i + 1 >= argc) {
                std::cerr << "--fps bir deger gerektirir\n";
                return 2;
            }

            unsigned int fps = 0;
            if (!parseUnsignedValue(argv[++i], fps)) {
                std::cerr << "gecersiz fps degeri\n";
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

        std::cerr << "Bilinmeyen arguman: " << arg << "\n";
        printUsage(std::cerr);
        return 2;
    }

    return app::run(config);
}

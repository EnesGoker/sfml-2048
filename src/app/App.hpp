#pragma once

#include <cstdint>
#include <optional>

namespace app {

struct RunConfig {
    std::optional<std::uint32_t> seed;
    bool vSyncEnabled{true};
    std::optional<unsigned int> frameLimit;
};

int run(const RunConfig &config = {});

} // namespace app

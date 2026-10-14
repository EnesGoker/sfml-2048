#include "core/ScoreStorage.hpp"

#include <utility>

namespace core2048 {

std::shared_ptr<ScoreStorage> makeScoreStorage() {
    return makeFileStorage();
}

std::shared_ptr<ScoreStorage>
makeScoreStorage(std::shared_ptr<detail::StorageFileOperations> operations) {
    return makeFileStorage(std::move(operations));
}

} // namespace core2048

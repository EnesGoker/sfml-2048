#pragma once

#include "core/FileStorage.hpp"

namespace core2048 {

// Preserve the score persistence API while sharing its model-independent backend.
using ScoreSaveStatus = FileSaveStatus;
using ScoreStorage = FileStorage;

std::shared_ptr<ScoreStorage> makeScoreStorage();
std::shared_ptr<ScoreStorage>
makeScoreStorage(std::shared_ptr<detail::StorageFileOperations> operations);

} // namespace core2048

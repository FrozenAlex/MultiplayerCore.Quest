#pragma once

#include "System/Object.hpp"
#include "custom-types/shared/macros.hpp"
#include "System/Threading/CancellationToken.hpp"
#include <functional>
#include <future>
#include <unordered_map>
#include <mutex>

DECLARE_CLASS_CODEGEN(MultiplayerCore::Objects, MpLevelDownloader, System::Object) {
    DECLARE_CTOR(ctor);
    DECLARE_SIMPLE_DTOR();
    public:
        std::shared_future<bool> TryDownloadLevelAsync(std::string levelId, std::function<void(double)> progress = nullptr);
    private:
        std::mutex downloadsMutex;
        std::unordered_map<std::string, std::shared_future<bool>> downloads;
        bool TryDownloadLevelInternal(std::string levelId, std::function<void(double)> progress = nullptr);
};

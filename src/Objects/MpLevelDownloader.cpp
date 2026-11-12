#include "beatsaber-hook/shared/threading.hpp"
#include "beatsaber-hook/shared/safeptr.hpp"
#include "Objects/MpLevelDownloader.hpp"
#include "beatsaverplusplus/shared/BeatSaver.hpp"
#include "songcore/shared/SongLoader/RuntimeSongLoader.hpp"
#include "songcore/shared/SongCore.hpp"
#include "logging.hpp"
#include "Utilities.hpp"

#include "lapiz/shared/utilities/MainThreadScheduler.hpp"
#include <chrono>
#include <cctype>      // std::tolower
#include <algorithm>   // std::equal

DEFINE_TYPE(MultiplayerCore::Objects, MpLevelDownloader);

namespace MultiplayerCore::Objects {
    void MpLevelDownloader::ctor() {
        INVOKE_CTOR();

    }

    std::shared_future<bool> MpLevelDownloader::TryDownloadLevelAsync(std::string levelId, std::function<void(double)> progress) {
        std::lock_guard lock(downloadsMutex);
        std::shared_future<bool> fut;
        auto dl = downloads.find(levelId);
        std::future_status status = std::future_status::timeout;
        if (dl != downloads.end()) {
            fut = dl->second;
            status = fut.wait_for(std::chrono::microseconds(0));
            if (status != std::future_status::ready)
                DEBUG("Download still in progress: {}", levelId);
            else if (status == std::future_status::ready && fut.get())
                DEBUG("Download already finished: {}", levelId);
        }

        // Recheck local presence: a previously downloaded map may have been deleted.
        if (dl == downloads.end() || (status == std::future_status::ready &&
            (!fut.get() || !SongCore::API::Loading::GetLevelByLevelID(levelId)))) {
            DEBUG("Starting download: {}", levelId);
            safe_ptr<MpLevelDownloader*> owner(this);
            downloads[levelId] = fut = il2cpp_async(std::launch::async, [owner, levelId, progress]() mutable {
                // A cached future may retain its callable after completion. Release
                // the roots when this invocation exits, including its progress owner.
                auto liveOwner = std::move(owner);
                auto liveProgress = std::move(progress);
                return liveOwner->TryDownloadLevelInternal(levelId, std::move(liveProgress));
            });
        }

        return fut;
    }

    bool MpLevelDownloader::TryDownloadLevelInternal(std::string levelId, std::function<void(double)> progress) {
        DEBUG("Called TryDownloadLevelInternal: {}", levelId);
        auto hash = HashFromLevelID(std::string_view(levelId));
        if (hash.empty()) {
            ERROR("Could not parse hash from id {}", levelId);
            return false;
        }

        auto bmRes = BeatSaver::API::GetBeatmapByHash(hash);
        if (bmRes.DataParsedSuccessful()) {
            const auto& versions = bmRes.responseData->GetVersions();
            const auto& v = std::find_if(versions.begin(), versions.end(),
                [hash = std::string_view(hash)](const auto& bv){ return bv.GetHash().size() == hash.size() && std::ranges::equal(bv.GetHash(), hash,
                    [](char a, char b){
                        return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
                    });
                }
            );
            if (v == versions.end()) {
                ERROR("Could not find version for hash {}", hash);
                return false;
            }
            auto completed = std::make_shared<std::promise<std::optional<std::filesystem::path>>>();
            auto result = completed->get_future();
            DEBUG("Starting download of beatmap: {}", hash);
            v->DownloadBeatmapAsync(*bmRes.responseData, [completed](const std::optional<std::filesystem::path>& path){
                completed->set_value(path);
            }, progress);

            auto downloadPath = result.get();
            if (!downloadPath || downloadPath->empty()) {
                WARNING("Download failed for {}", bmRes.responseData->GetId());
                return false;
            }

            DEBUG("Download finished successfully to '{}', refreshing songs now", downloadPath->string());
            SongCore::API::Loading::RefreshSongs(false).wait();

            auto level = SongCore::API::Loading::GetLevelByHash(hash);
            DEBUG("Got level: {}", fmt::ptr(level));
            return level != nullptr;
        } else {
            ERROR("Couldn't get beatmap by hash: {}", hash);
            return false;
        }
    }
}

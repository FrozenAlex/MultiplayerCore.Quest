#include "logging.hpp"
#include "hooking.hpp"
#include "tasks.hpp"
#include "Utilities.hpp"

#include "beatsaverplusplus/shared/BeatSaver.hpp"
#include "songcore/shared/SongCore.hpp"

#include "GlobalNamespace/BeatmapLevelsModel.hpp"
#include "GlobalNamespace/LoadBeatmapLevelDataResult.hpp"

MAKE_AUTO_HOOK_MATCH(BeatmapLevelsModel_CheckBeatmapLevelDataExistsAsync, &GlobalNamespace::BeatmapLevelsModel::CheckBeatmapLevelDataExistsAsync, 
    System::Threading::Tasks::Task_1<bool>*, 
    GlobalNamespace::BeatmapLevelsModel* self, 
    ::StringW levelID,
    ::GlobalNamespace::BeatmapLevelDataVersion beatmapLevelDataVersion,
    ::System::Threading::CancellationToken cancelToken) {

        auto t = BeatmapLevelsModel_CheckBeatmapLevelDataExistsAsync(self, levelID, beatmapLevelDataVersion, cancelToken);
        // check for levelhash -> no hash means base game, return orig
        auto levelHash = MultiplayerCore::HashFromLevelID(levelID);
        if (levelHash.empty()) return t;

        // check if locally available, if so return orig
        if (SongCore::API::Loading::GetLevelByHash(levelHash)) return t;

        // custom level, not locally available, get on beatsaver
        safe_ptr<System::Threading::Tasks::Task_1<bool>*> original(t);
        return MultiplayerCore::StartTask<bool>([original, levelHash = std::string(levelHash)](System::Threading::CancellationToken token){
            using namespace std::chrono_literals;
            // if orig says it's available, cool beans
            if (MultiplayerCore::Await(original.ptr(), token)) return true;

            // check if map is on beatsaver
            auto beatmapRes = BeatSaver::API::GetBeatmapByHash(levelHash);
            if (beatmapRes.DataParsedSuccessful()) {
                for (auto& v : beatmapRes.responseData->GetVersions()) {
                    if (v.GetHash() == levelHash) {
                        auto completed = std::make_shared<std::promise<bool>>();
                        auto downloaded = completed->get_future();
                        v.DownloadBeatmapAsync(*beatmapRes.responseData, [completed](bool success){
                            completed->set_value(success);
                        });
                        if (!downloaded.get()) return false;
                        token.ThrowIfCancellationRequested();

                        SongCore::API::Loading::RefreshSongs().wait();

                        auto level = SongCore::API::Loading::GetLevelByHash(levelHash);
                        if (level) return true;
                        break;
                    }
                }
            }
            return false;
        }, cancelToken);
}

#include "beatsaber-hook/shared/threading.hpp"
#include "beatsaber-hook/shared/safeptr.hpp"
#include <mutex>
#include "Beatmaps/Providers/MpBeatmapLevelProvider.hpp"
#include "Beatmaps/NetworkBeatmapLevel.hpp"
#include "Beatmaps/LocalBeatmapLevel.hpp"
#include "Beatmaps/BeatSaverBeatmapLevel.hpp"
#include "Beatmaps/BeatSaverPreviewMediaData.hpp"

#include "beatsaber-hook/shared/api.hpp"
#include "beatsaber-hook/shared/members.hpp"
#include "beatsaber-hook/shared/types.hpp"
#include "beatsaber-hook/shared/utils.hpp"
#include "songcore/shared/SongCore.hpp"
#include "beatsaverplusplus/shared/BeatSaver.hpp"

#include "GlobalNamespace/EnvironmentName.hpp"
#include "GlobalNamespace/BeatmapBasicData.hpp"
#include "System/ValueTuple_2.hpp"

#include "Utilities.hpp"
#include "logging.hpp"
#include "System/ValueTuple_3.hpp"

DEFINE_TYPE(MultiplayerCore::Beatmaps::Providers, MpBeatmapLevelProvider);

namespace MultiplayerCore::Beatmaps::Providers {
    static std::mutex beatsaverCacheMutex;
    void MpBeatmapLevelProvider::ctor() {
        INVOKE_CTOR();
        _hashToNetworkLevels = HashToLevelDict::New_ctor();
        _hashToBeatsaverLevels = HashToLevelDict::New_ctor();
    }

    std::future<GlobalNamespace::BeatmapLevel*> MpBeatmapLevelProvider::GetBeatmapAsync(const std::string& levelHash) {
        auto map = GetBeatmapFromLocalBeatmaps(levelHash);
        if (map) return finished_future(map);
        return GetBeatmapFromBeatSaverAsync(levelHash);
    }

    std::future<GlobalNamespace::BeatmapLevel*> MpBeatmapLevelProvider::GetBeatmapFromBeatSaverAsync(const std::string& levelHash) {
        safe_ptr<MpBeatmapLevelProvider*> owner(this);
        return il2cpp_async(std::launch::async, [owner, levelHash]() mutable {
            auto liveOwner = std::move(owner);
            return liveOwner->GetBeatmapFromBeatSaver(levelHash);
        });
    }

    GlobalNamespace::BeatmapLevel* MpBeatmapLevelProvider::GetBeatmapFromBeatSaver(std::string levelHash) {
        GlobalNamespace::BeatmapLevel* level = nullptr;
        {
            std::lock_guard lock(beatsaverCacheMutex);
            if (_hashToBeatsaverLevels->TryGetValue(levelHash, by_ref(level))) return level;
        }

        auto beatmapRes = BeatSaver::API::GetBeatmapByHash(static_cast<std::string>(levelHash));
        if (beatmapRes.DataParsedSuccessful()) {
            auto result  = beatmapRes.responseData.value();

            if (result.GetVersions().empty()) {
                DEBUG("BeatSaver returned level '{}' did not contain any versions!!!", levelHash);
                return nullptr;
            }
            // Test check Diffs
            auto diffs = result.GetVersions().front().GetDiffs();
            if (diffs.empty()) {
                DEBUG("BPP returned level '{}' did not contain diffs!!!", levelHash);
            } else {
                for (auto& diff : diffs) {
                    DEBUG("Diff: {}", diff.GetDifficulty());
                }
            }
            try {
                std::lock_guard lock(beatsaverCacheMutex);
                if (_hashToBeatsaverLevels->TryGetValue(levelHash, by_ref(level))) return level;
                level = BeatSaverBeatmapLevel::Make(levelHash, result);
                _hashToBeatsaverLevels->Add(levelHash, level);
                return level;
            }
            catch (const std::exception& e) {
                // Fuck, had this happen a few times and then not, so idk what's going on there
                CRITICAL("IMPORTANT FORWARD TO MpCore DEVS IF YOU SEE THIS!!! - Failed to create BeatSaverBeatmapLevel: {}", e.what());
                Paper::Logger::Backtrace(MOD_ID, 20);
                return nullptr;
            }
        }

        return nullptr;
    }

    GlobalNamespace::BeatmapLevel* MpBeatmapLevelProvider::GetBeatmapFromLocalBeatmaps(const std::string& levelHash) {
        auto customLevel = SongCore::API::Loading::GetLevelByHash(levelHash);
        if (!customLevel) return nullptr;
        return Beatmaps::LocalBeatmapLevel::New_ctor(levelHash, customLevel);
    }

    GlobalNamespace::BeatmapLevel* MpBeatmapLevelProvider::GetBeatmapFromPacket(Packets::MpBeatmapPacket* packet) {
        // cache network levels
        GlobalNamespace::BeatmapLevel* level = nullptr;
        if (_hashToNetworkLevels->TryGetValue(packet->levelHash, by_ref(level))) {
            return level;
        }

        level = NetworkBeatmapLevel::New_ctor(packet);
        _hashToNetworkLevels->Add(packet->levelHash, level);
        return level;
    }

    GlobalNamespace::BeatmapLevel* MpBeatmapLevelProvider::TryGetBeatmapFromPacketHash(std::string levelHash) {
        GlobalNamespace::BeatmapLevel* level = nullptr;
        if (_hashToNetworkLevels->TryGetValue(levelHash, by_ref(level))) {
            return level;
        }
        return nullptr;
    }

    GlobalNamespace::BeatmapLevel* MpBeatmapLevelProvider::AddBasicBeatmapDataToLevel(GlobalNamespace::BeatmapLevel* level, const GlobalNamespace::BeatmapKey& beatmapKey, Packets::MpBeatmapPacket* packet) {
        using BasicDataDict = System::Collections::Generic::Dictionary_2<System::ValueTuple_3<GlobalNamespace::GameplayType, GlobalNamespace::BeatmapCharacteristic, GlobalNamespace::BeatmapDifficulty>, GlobalNamespace::BeatmapBasicData*>;
        BasicDataDict* dict = reinterpret_cast<BasicDataDict*>(level->_gameplayTypeBeatmapBasicDatas);

        if (!dict) {
            dict = BasicDataDict::New_ctor();
            level->_gameplayTypeBeatmapBasicDatas = dict;
        }

        auto key = System::ValueTuple_3<GlobalNamespace::GameplayType, GlobalNamespace::BeatmapCharacteristic, GlobalNamespace::BeatmapDifficulty>(
            beatmapKey.gameplayType, beatmapKey.characteristic,
            beatmapKey.difficulty
        );

        // TODO: Figure out why when using BeatSaverBeatmapLevel, allMappers and allLighters cause a segfault
        if (!dict->ContainsKey(key)) {
            dict->Add(
                key,
                GlobalNamespace::BeatmapBasicData::New_ctor(
                    0, 0, GlobalNamespace::EnvironmentName::getStaticF_Empty(),
                    nullptr, 0, 0, 0, 0,
                    (level->allMappers.size() > 0 ? level->allMappers : std::initializer_list<StringW>{packet ? packet->levelAuthorName : ""}), level->allLighters

                )
            );
        }

        level->_gameplayTypeBeatmapBasicDatas = dict;
        return level;
    }
}

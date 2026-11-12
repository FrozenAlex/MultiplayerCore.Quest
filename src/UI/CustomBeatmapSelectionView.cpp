#include "UI/CustomBeatmapSelectionView.hpp"
#include "Beatmaps/Packets/MpBeatmapPacket.hpp"
#include "Beatmaps/Providers/MpBeatmapLevelProvider.hpp"
#include "GlobalNamespace/LevelBar.hpp"
#include "UI/MpRequirementsUI.hpp"
#include "UnityEngine/GameObject.hpp"
#include "Utilities.hpp"
#include "logging.hpp"
#include "GlobalNamespace/BeatmapCharacteristicExtensions.hpp"

DEFINE_TYPE(MultiplayerCore::UI, CustomBeatmapSelectionView);

namespace MultiplayerCore::UI {

    void CustomBeatmapSelectionView::Inject(Objects::MpPlayersDataModel* mpPlayersDataModel, Beatmaps::Providers::MpBeatmapLevelProvider* mpBeatmapLevelProvider) {
        _mpPlayersDataModel = mpPlayersDataModel;
        _mpBeatmapLevelProvider = mpBeatmapLevelProvider;
    }

    void CustomBeatmapSelectionView::SetBeatmap(by_ref<const GlobalNamespace::BeatmapKey> beatmapKey) {
        // The generated virtual wrapper dispatches back to this override, even
        // with C++ base qualification. Invoke the base MethodInfo explicitly.
        static auto method = i2c::metadata_getter<&GlobalNamespace::BeatmapSelectionView::SetBeatmap>::method_info();
        if (!GlobalNamespace::BeatmapKey(*beatmapKey).IsValid()) {
            WARNING("beatmapkey was invalid, {} {} {}", beatmapKey->levelId, (int)beatmapKey->difficulty, GlobalNamespace::BeatmapCharacteristicExtensions::SerializedName(beatmapKey->characteristic));
            return cordl_internals::RunMethodRethrow<void>(this, method, beatmapKey);
        }

        auto levelHash = HashFromLevelID(beatmapKey->levelId);
        if (levelHash.empty()) { // level wasn't custom
            DEBUG("Level {} wasn't custom", beatmapKey->levelId);
            return cordl_internals::RunMethodRethrow<void>(this, method, beatmapKey);
        }

        StartCoroutine(custom_types::Helpers::CoroutineHelper::New(SetBeatmapCoroutine((*beatmapKey), levelHash)));
    }

    custom_types::Helpers::Coroutine CustomBeatmapSelectionView::SetBeatmapCoroutine(GlobalNamespace::BeatmapKey beatmapKey, std::string levelHash) {
        auto packet = _mpPlayersDataModel->FindLevelPacket(levelHash);
        GlobalNamespace::BeatmapLevel* level = nullptr;
        std::shared_future<GlobalNamespace::BeatmapLevel*> fut;
        if (!packet) { // level was probably selected by the local player from their installed maps, since no other player had it selected
            DEBUG("Could not find packet for levelhash {}, falling back to beatsaver", levelHash);
            fut = _mpBeatmapLevelProvider->GetBeatmapAsync(levelHash);
            while (fut.valid() && fut.wait_for(std::chrono::seconds(0)) != std::future_status::ready) co_yield nullptr;
            level = fut.get();
        } else {
            // create a beatmap level for display
            level = _mpBeatmapLevelProvider->GetBeatmapFromPacket(packet);
        }

        if (!level) {
            WARNING("Couldn't get level, returning...");
            co_return;
        }
        
        // we have a packet to select from, update values correctly without calling base, and setup the levelbar with our own level
        _noLevelText->enabled = false;
        _levelBar->hide = false;

        level = _mpBeatmapLevelProvider->AddBasicBeatmapDataToLevel(level, beatmapKey, packet);

        _levelBar->Setup(level, beatmapKey.difficulty, beatmapKey.characteristic);
        co_return;
    }
}

#include "Beatmaps/NoInfoBeatmapLevel.hpp"
#include "Beatmaps/BeatSaverPreviewMediaData.hpp"
#include "System/Collections/Generic/Dictionary_2.hpp"
#include "System/ValueTuple_3.hpp"

DEFINE_TYPE(MultiplayerCore::Beatmaps, NoInfoBeatmapLevel);

namespace MultiplayerCore::Beatmaps {
    void NoInfoBeatmapLevel::ctor_1(StringW hash) {
        INVOKE_CTOR();
        INVOKE_BASE_CTOR(i2c::class_of<Abstractions::MpBeatmapLevel*>());

        set_levelHash(static_cast<std::string>(hash));
        static ConstString Empty("");

        _ctor(
            -1,
            false,
            fmt::format("custom_level_{}", hash),
            Empty,
            Empty,
            Empty,
            ArrayW<StringW>::New(),
            ArrayW<StringW>::New(),
            0,
            0,
            0,
            0,
            0,
            0,
            GlobalNamespace::PlayerSensitivityFlag::Safe,
            BeatSaverPreviewMediaData::New_ctor(hash)->i_IPreviewMediaData(),
            System::Collections::Generic::Dictionary_2<System::ValueTuple_3<GlobalNamespace::GameplayType, GlobalNamespace::BeatmapCharacteristic, GlobalNamespace::BeatmapDifficulty>, GlobalNamespace::BeatmapBasicData*>::New_ctor()
        );
    }
}

#include "Networking/MultiplayerTypes.hpp"
#include "GlobalNamespace/BeatmapCharacteristicExtensions.hpp"
#include "Objects/MpPlayersDataModel.hpp"
#include "Beatmaps/Packets/MpBeatmapPacket.hpp"
#include "logging.hpp"
#include "Utilities.hpp"

#include "System/Collections/Generic/Dictionary_2.hpp"
#include "GlobalNamespace/BeatmapCharacteristicCollectionSO.hpp"
#include "GlobalNamespace/BeatmapCharacteristicCollection.hpp"
#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "GlobalNamespace/LobbyPlayerData.hpp"
#include "GlobalNamespace/IBeatSaberMultiplayerSessionManager.hpp"

DEFINE_TYPE(MultiplayerCore::Objects, MpPlayersDataModel);

using namespace MultiplayerCore::Beatmaps::Packets;

namespace MultiplayerCore::Objects {
    void MpPlayersDataModel::ctor(Networking::MpPacketSerializer* packetSerializer, Beatmaps::Providers::MpBeatmapLevelProvider* beatmapLevelProvider) {
        INVOKE_CTOR();
        Base::_ctor();

        _packetSerializer = packetSerializer;
        _beatmapLevelProvider = beatmapLevelProvider;
        _lastPlayerBeatmapPackets = PacketDict::New_ctor();
    }

    void MpPlayersDataModel::Activate_override() {
        DEBUG("Activate");
        _packetSerializer->RegisterCallback<MpBeatmapPacket*>(std::bind(&MpPlayersDataModel::HandleMpCoreBeatmapPacket, this, std::placeholders::_1, std::placeholders::_2));
        Base::Activate();
    }

    void MpPlayersDataModel::Deactivate_override() {
        DEBUG("Deactivate");
        _packetSerializer->UnregisterCallback<MpBeatmapPacket*>();
        Base::Deactivate();
    }

    void MpPlayersDataModel::Dispose() {
        Deactivate();
    }

    void MpPlayersDataModel::HandlePlayerConnected(GlobalNamespace::IConnectedPlayer* connectedPlayer) {
        Base::HandleMultiplayerSessionManagerPlayerConnected(MultiplayerCore::ConnectedPlayer(connectedPlayer));
        GlobalNamespace::LobbyPlayerData* localPlayerData = nullptr;
        if (_playersData->TryGetValue(localUserId, by_ref(localPlayerData)) && localPlayerData) {
            SendMpBeatmapPacket(localPlayerData->beatmapKey, i2c::try_cast<GlobalNamespace::IBeatSaberConnectedPlayer*>(connectedPlayer));
        }
    }

    void MpPlayersDataModel::HandleMpCoreBeatmapPacket(MpBeatmapPacket* packet, GlobalNamespace::IBeatSaberConnectedPlayer* player) {
        DEBUG("'{}' selected song '{}'", MultiplayerCore::ConnectedPlayer(player)->get_userId(), packet->levelHash);
        GlobalNamespace::BeatmapCharacteristic ch{};
        if (!GlobalNamespace::BeatmapCharacteristicExtensions::BeatmapCharacteristicFromSerializedName(packet->characteristic, by_ref(ch))) return;
        auto beatmapKey = GlobalNamespace::BeatmapKey(ch, packet->difficulty, fmt::format("custom_level_{}", packet->levelHash), GlobalNamespace::GameplayType::BeatSaber);

        PutPlayerPacket(MultiplayerCore::ConnectedPlayer(player)->get_userId(), packet);
        SetPlayerBeatmapLevel(MultiplayerCore::ConnectedPlayer(player)->get_userId(), beatmapKey);
    }

    void MpPlayersDataModel::HandleMenuRpcManagerGetRecommendedBeatmap_override(StringW userId) {
        GlobalNamespace::LobbyPlayerData* localPlayerData = nullptr;
        if (_playersData->TryGetValue(userId, by_ref(localPlayerData)) && localPlayerData) {
            SendMpBeatmapPacket(localPlayerData->beatmapKey, nullptr);
        }

        Base::HandleMenuRpcManagerGetRecommendedBeatmap(userId);
    }

    void MpPlayersDataModel::HandleMenuRpcManagerRecommendBeatmap_override(StringW userId, GlobalNamespace::BeatmapKeyNetSerializable* beatmapKey) {
        auto hash = HashFromLevelID(beatmapKey->levelID);
        if (!hash.empty()) return; // if this is a custom level return

        Base::HandleMenuRpcManagerRecommendBeatmap(userId, beatmapKey);
    }

    void MpPlayersDataModel::SetLocalPlayerBeatmapLevel_override(const GlobalNamespace::BeatmapKey& beatmapKey) {
        DEBUG("Setting local player beatmap level id '{}'\r\nCheck difficulty '{}'\r\nCheck characteristic '{}'\r\nCheck BeatmapKey Valid '{}'", beatmapKey.levelId, (int)beatmapKey.difficulty, std::string(::GlobalNamespace::BeatmapCharacteristicExtensions::SerializedName(beatmapKey.characteristic)), GlobalNamespace::BeatmapKey(beatmapKey).IsValid());
        SendMpBeatmapPacket(beatmapKey, nullptr);

        Base::SetLocalPlayerBeatmapLevel(by_ref(beatmapKey));
    }

    void MpPlayersDataModel::SendMpBeatmapPacket(GlobalNamespace::BeatmapKey beatmapKey, GlobalNamespace::IBeatSaberConnectedPlayer* player) {
        DEBUG("Sending mp beatmap packet for level '{}'", beatmapKey.levelId);

        auto levelId = beatmapKey.levelId;
        auto hash = HashFromLevelID(levelId);
        if (hash.empty()) {
            DEBUG("not a custom level, returning...");
            return;
        }

        auto levelData = _beatmapLevelProvider->GetBeatmapFromLocalBeatmaps(hash);
        auto packet = levelData ? MpBeatmapPacket::New_1(levelData, beatmapKey) : FindLevelPacket(hash);
        if (!packet) {
            WARNING("Could not get level data for beatmap '{}', returning!", hash);
            return;
        }

        // Check we have a value here, otherwise per player difficulty will break, 
        // honestly should just add difficulties to the packet, instead of relying on the requirements
        if (packet->requirements.empty() || !packet->requirements.contains(beatmapKey.difficulty.value__)) {
            ERROR("Could not determine difficulties for level, things may break or not work as expected");
        }


        if (player) {
            _packetSerializer->SendToPlayer(packet, player);
        } else {
            _packetSerializer->Send(packet);
        }
    }

    Beatmaps::Packets::MpBeatmapPacket* MpPlayersDataModel::GetPlayerPacket(StringW playerId) {
        Beatmaps::Packets::MpBeatmapPacket* value = nullptr;
        _lastPlayerBeatmapPackets->TryGetValue(playerId, by_ref(value));
        return value;
    }

    void MpPlayersDataModel::PutPlayerPacket(StringW playerId, Beatmaps::Packets::MpBeatmapPacket* packet) {
        _lastPlayerBeatmapPackets->set_Item(playerId, packet);
    }

    Beatmaps::Packets::MpBeatmapPacket* MpPlayersDataModel::FindLevelPacket(std::string_view levelHash) {
        // Uppercase the hash we get just in case
        auto upperHash = std::string(levelHash);
        std::transform(upperHash.begin(), upperHash.end(), upperHash.begin(), toupper);

        Beatmaps::Packets::MpBeatmapPacket* packet = nullptr;
        auto enumerator = _lastPlayerBeatmapPackets->GetEnumerator();
        while (enumerator.MoveNext()) {
            auto [_, p] = enumerator.Current;
            std::string packetHash(p->levelHash);
            DEBUG("Found packetHash {}", packetHash);
            std::transform(packetHash.begin(), packetHash.end(), packetHash.begin(), toupper);
            if (packetHash == upperHash) {
                packet = p;
                break;
            }
        }
        enumerator.Dispose();
        return packet;
    }
}

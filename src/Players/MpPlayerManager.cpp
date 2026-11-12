#include "Networking/MultiplayerTypes.hpp"
#include "Players/MpPlayerManager.hpp"
#include "OculusStudios/Platform/Core/IPlatformUser.hpp"
#include "custom-types/shared/delegate.hpp"
// #include "bsml/shared/Helpers/delegates.hpp"

#include "bsml/shared/BSML/SharedCoroutineStarter.hpp"
#include "System/Threading/Tasks/Task_1.hpp"

DEFINE_TYPE(MultiplayerCore::Players, MpPlayerManager);

namespace MultiplayerCore::Players {
    void MpPlayerManager::ctor(MultiplayerCore::Networking::MpPacketSerializer* packetSerializer, OculusStudios::Platform::Core::IPlatform* platform, GlobalNamespace::IBeatSaberMultiplayerSessionManager* sessionManager) {
        INVOKE_CTOR();

        _playerData = ConcurrentPlayerDataDictionary::New_ctor();

        _packetSerializer = packetSerializer;
        _platform = platform;
        _sessionManager = MultiplayerCore::Session(sessionManager);

    }

    void MpPlayerManager::Initialize() {
        // UserInfo is no longer a Zenject binding in 1.45. Read the identity from
        // the initialized platform service, matching the game's current API.
        const std::string key(_platform->get_key());
        const auto platform = (key == "oculus" || key == "oculus-mock")
            ? GlobalNamespace::UserInfo::Platform::Oculus
            : key == "pico" ? GlobalNamespace::UserInfo::Platform(20)
            : GlobalNamespace::UserInfo::Platform::Test;
        auto user = _platform->get_user();
        _localPlayerInfo = GlobalNamespace::UserInfo::New_ctor(platform, std::to_string(user->get_userId()), user->get_displayName());
        _sessionManager->SetLocalPlayerState("modded", true);
        _packetSerializer->RegisterCallback<MpPlayerData*>(
            std::bind(&MpPlayerManager::HandlePlayerData, this, std::placeholders::_1, std::placeholders::_2)
        );
        _playerConnectedDelegate = custom_types::MakeDelegate<System::Action_1<GlobalNamespace::IBeatSaberConnectedPlayer*>*>(
            this, std::function<void(MpPlayerManager*, GlobalNamespace::IBeatSaberConnectedPlayer*)>(&MpPlayerManager::HandlePlayerConnected));
        _sessionManager->add_playerConnectedEvent(_playerConnectedDelegate);
    }


    void MpPlayerManager::Dispose() {
        if (_playerConnectedDelegate) _sessionManager->remove_playerConnectedEvent(_playerConnectedDelegate);
        _playerConnectedDelegate = nullptr;
        _packetSerializer->UnregisterCallback<MpPlayerData*>();
    }

    void MpPlayerManager::HandlePlayerConnected(GlobalNamespace::IBeatSaberConnectedPlayer* player) {
        if (!_localPlayerInfo) throw std::runtime_error("local player info was not yet set! make sure it is set before anything else happens!");

        auto packet = MpPlayerData::New_ctor();
        packet->platformId = _localPlayerInfo->platformUserId;
        // we're either in testing, so unknown, or we're regular user on OculusQuest or Pico
        // packet->platform = _localPlayerInfo->platform == GlobalNamespace::UserInfo::Platform::Test ? MultiplayerCore::Players::Platform::Unknown : MultiplayerCore::Players::Platform::OculusQuest;
        if (_localPlayerInfo->platform == GlobalNamespace::UserInfo::Platform::Test) {
            packet->platform = MultiplayerCore::Players::Platform::Unknown;
        } else if (_localPlayerInfo->platform.value__ == 20) {
            packet->platform = MultiplayerCore::Players::Platform::Pico;
        }
        else if (_localPlayerInfo->platform == GlobalNamespace::UserInfo::Platform::Oculus) {
            packet->platform = MultiplayerCore::Players::Platform::OculusQuest;
        } else {
            packet->platform = MultiplayerCore::Players::Platform::Unknown;
        }
        _packetSerializer->Send(packet);
    }

    void MpPlayerManager::HandlePlayerData(MpPlayerData* packet, GlobalNamespace::IBeatSaberConnectedPlayer* player) {
        _playerData->set_Item(MultiplayerCore::ConnectedPlayer(player)->get_userId(), packet);
        PlayerConnectedEvent.invoke(player, packet);
    }

    bool MpPlayerManager::TryGetPlayer(StringW userId, MpPlayerData*& outplayer) {
        if (_playerData->ContainsKey(userId)) {
            outplayer = _playerData->get_Item(userId);
            return true;
        }
        return false;
    }

    MpPlayerData* MpPlayerManager::GetPlayer(StringW userId) {
        MpPlayerData* player = nullptr;
        TryGetPlayer(userId, player);
        return player;
    }
}

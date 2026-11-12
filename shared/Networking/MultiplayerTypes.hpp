#pragma once
#include "GlobalNamespace/IBeatSaberMultiplayerSessionManager.hpp"
#include "GlobalNamespace/IMultiplayerSessionManager_1.hpp"
#include "GlobalNamespace/IBeatSaberConnectedPlayer.hpp"
#include "GlobalNamespace/IConnectedPlayer.hpp"

namespace MultiplayerCore {
    using SessionManager = GlobalNamespace::IMultiplayerSessionManager_1<GlobalNamespace::IBeatSaberConnectedPlayer*>;

    inline SessionManager* Session(GlobalNamespace::IBeatSaberMultiplayerSessionManager* session) {
        return session ? static_cast<SessionManager*>(*session) : nullptr;
    }
    inline GlobalNamespace::IConnectedPlayer* ConnectedPlayer(GlobalNamespace::IBeatSaberConnectedPlayer* player) {
        return player ? static_cast<GlobalNamespace::IConnectedPlayer*>(*player) : nullptr;
    }
    inline GlobalNamespace::IConnectedPlayer* ConnectedPlayer(GlobalNamespace::IConnectedPlayer* player) { return player; }
}

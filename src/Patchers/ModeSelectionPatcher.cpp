#include "Patchers/ModeSelectionPatcher.hpp"
#include "Hooks/NetworkConfigHooks.hpp"

DEFINE_TYPE(MultiplayerCore::Patchers, ModeSelectionPatcher);

namespace MultiplayerCore::Patchers {
    ModeSelectionPatcher* ModeSelectionPatcher::instance;
    ModeSelectionPatcher* ModeSelectionPatcher::get_instance() { return instance; }

    void ModeSelectionPatcher::ctor(GlobalNamespace::INetworkConfig* networkConfig) {
        INVOKE_CTOR();
        _networkConfig = networkConfig;
        instance = this;
        lastStatusUrl = "";
    }

    void ModeSelectionPatcher::Dispose() {
        if (instance == this) instance = nullptr;
    }

    bool ModeSelectionPatcher::UpdateStatusUrl() {
        auto statusUrl = StringW(MultiplayerCore::Hooks::NetworkConfigHooks::GetCurrentServer()->masterServerStatusUrl);
        if (statusUrl == lastStatusUrl) return false;
        lastStatusUrl = statusUrl;
        return true;
    }

    StringW ModeSelectionPatcher::get_lastStatusUrl() { return lastStatusUrl; }
}

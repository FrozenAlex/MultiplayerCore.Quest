#include "beatsaber-hook/shared/callback.hpp"
#include "Hooks/NetworkConfigHooks.hpp"
#include "logging.hpp"
#include "hooking.hpp"

#include "GlobalNamespace/NetworkConfigSO.hpp"
#include "GlobalNamespace/MainSettingsAsyncLoader.hpp"

#include "GlobalNamespace/UnifiedNetworkPlayerModel.hpp"
#include "IgnoranceCore/IgnoranceClient.hpp"
#include "System/Security/Cryptography/X509Certificates/X509Certificate2.hpp"

#define LOG_VALUE(identifier, value) DEBUG("Overriding NetworkConfigSO::" identifier " to '{}'", cfg->value)

// File is equivalent to MultiplayerCore.Patchers.NetworkConfigPatcher from PC

namespace MultiplayerCore::Hooks {
    ServerConfig NetworkConfigHooks::officialServerConfig = {"https://graph.oculus.com", "https://graph.oculus.com/beat_saber_multiplayer_status", OFFICIAL_MAX_PARTY_SIZE, "", false };
    const ServerConfig* NetworkConfigHooks::currentServerConfig{};
    GlobalNamespace::NetworkConfigSO* NetworkConfigHooks::networkConfig = nullptr;
    static bool originalUseLocalServer = false;
    unordered_event_callback<const ServerConfig*> NetworkConfigHooks::ServerChanged{};

    const ServerConfig* NetworkConfigHooks::GetCurrentServer() { return currentServerConfig; }
    const ServerConfig* NetworkConfigHooks::GetOfficialServer() { return &officialServerConfig; }

    bool NetworkConfigHooks::IsOverridingAPI() { return GetCurrentServer() && GetCurrentServer() != GetOfficialServer(); }

    void NetworkConfigHooks::UseCustomApiServer(const std::string& graphUrl, const std::string& statusUrl, int maxPartySize, const std::string& quickPlaySetupUrl, bool disableSSL) {
        static ServerConfig tempConfig;
        if (graphUrl.empty() || statusUrl.empty()) {
            DEBUG("Invalid server config, using official server");
            UseOfficialServer();
            return;
        }

        tempConfig = ServerConfig(graphUrl, statusUrl, maxPartySize, quickPlaySetupUrl, disableSSL);
        UseServer(&tempConfig);
    }

    void NetworkConfigHooks::UseServer(const ServerConfig* cfg) {
        if (!cfg) {
            DEBUG("Invalid server config, using official server");
            UseOfficialServer();
            return;
        }

        currentServerConfig = cfg;

        if (networkConfig) {
            networkConfig->_graphUrl = cfg->graphUrl;
            LOG_VALUE("graphUrl", graphUrl);

            networkConfig->_quickPlaySetupUrl = cfg->quickPlaySetupUrl;
            LOG_VALUE("quickPlaySetupUrl", quickPlaySetupUrl);

            // only 128 ids can exist, 0 & 127 are taken as server and broadcast
            // thus 128 - 2 = 126 absolute max player count
            networkConfig->_maxPartySize = std::clamp(cfg->maxPartySize, 2, 126);
            LOG_VALUE("maxPartySize", maxPartySize);
            networkConfig->_discoveryPort = cfg->discoveryPort;
            LOG_VALUE("discoveryPort", discoveryPort);
            networkConfig->_partyPort = cfg->partyPort;
            LOG_VALUE("partyPort", partyPort);
            networkConfig->_multiplayerPort = cfg->multiplayerPort;
            LOG_VALUE("multiplayerPort", multiplayerPort);
            networkConfig->_useLocalServer = IsOverridingAPI() ? false : originalUseLocalServer;
        }

        ServerChanged.invoke(cfg);
    }

    void NetworkConfigHooks::UseOfficialServer() {
        if (!networkConfig) {
            ERROR("NetworkConfigSO is null, official server not set");
            return;
        }
        UseServer(&officialServerConfig);
    }
}

using namespace MultiplayerCore::Hooks;

void NetworkConfigHooks::Initialize(GlobalNamespace::NetworkConfigSO* config) {
    if (!config || networkConfig == config) return;
    networkConfig = config;
    officialServerConfig.graphUrl = static_cast<std::string>(config->_graphUrl);
    officialServerConfig.masterServerStatusUrl = officialServerConfig.graphUrl + "/beat_saber_multiplayer_status";
    officialServerConfig.quickPlaySetupUrl = static_cast<std::string>(config->_quickPlaySetupUrl);
    officialServerConfig.maxPartySize = config->_maxPartySize;
    officialServerConfig.discoveryPort = config->_discoveryPort;
    officialServerConfig.partyPort = config->_partyPort;
    officialServerConfig.multiplayerPort = config->_multiplayerPort;
    originalUseLocalServer = config->_useLocalServer;
    UseServer(currentServerConfig ? currentServerConfig : &officialServerConfig);
}

// 1.45 loads network settings asynchronously and registers their installer separately.
// Initialize the asset before it is exposed to any networking consumer; resolving
// INetworkConfig from MainSystemInit is too early, before that binding exists.
MAKE_AUTO_HOOK_ORIG_MATCH(MainSettingsAsyncLoader_RegisterInstallers, &GlobalNamespace::MainSettingsAsyncLoader::RegisterInstallers, void,
    GlobalNamespace::MainSettingsAsyncLoader* self, BGLib::AppFlow::Initialization::IInstallerRegistry* registry) {
    NetworkConfigHooks::Initialize(self->_networkConfig);
    MainSettingsAsyncLoader_RegisterInstallers(self, registry);
    INFO("Network configuration initialized before settings installer registration");
}

MAKE_AUTO_HOOK_MATCH(UnifiedNetworkPlayerModel_SetActiveNetworkPlayerModelType, &GlobalNamespace::UnifiedNetworkPlayerModel::SetActiveNetworkPlayerModelType, void, GlobalNamespace::UnifiedNetworkPlayerModel* self, GlobalNamespace::UnifiedNetworkPlayerModel::ActiveNetworkPlayerModelType activeNetworkPlayerModelType) {
    if (NetworkConfigHooks::IsOverridingAPI()) {
        DEBUG("Disabling MasterServer, Setting to GameLift");
        UnifiedNetworkPlayerModel_SetActiveNetworkPlayerModelType(self, GlobalNamespace::UnifiedNetworkPlayerModel::ActiveNetworkPlayerModelType::GameLift);
        return;
    }

    DEBUG("Using Default");
    UnifiedNetworkPlayerModel_SetActiveNetworkPlayerModelType(self, activeNetworkPlayerModelType);
}

MAKE_AUTO_HOOK_MATCH(IgnoranceClient_Start, &IgnoranceCore::IgnoranceClient::Start, void, IgnoranceCore::IgnoranceClient* self) {
    auto cfg = NetworkConfigHooks::GetCurrentServer();
    DEBUG("Changing IgnoranceClient ssl usage");

    self->UseSsl = !(cfg && cfg->disableSSL);
    self->ValidateCertificate = !NetworkConfigHooks::IsOverridingAPI();

    IgnoranceClient_Start(self);
}

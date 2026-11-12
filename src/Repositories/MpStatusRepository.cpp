#include "Repositories/MpStatusRepository.hpp"
#include "logging.hpp"
#include "Hooks/NetworkConfigHooks.hpp"

DEFINE_TYPE(MultiplayerCore::Repositories, MpStatusRepository);

namespace MultiplayerCore::Repositories {
    MpStatusRepository* MpStatusRepository::_instance = nullptr;

    void MpStatusRepository::New(GlobalNamespace::INetworkConfig* networkConfig) {
        INVOKE_CTOR();
        _statusByUrl = System::Collections::Generic::Dictionary_2<StringW, MultiplayerCore::Models::MpStatusData*>::New_ctor();
        _networkConfig = networkConfig;
        _instance = this;
    }

    MpStatusRepository* MpStatusRepository::get_Instance() {
        if (!_instance) {
            ERROR("MpStatusRepository instance is null! Did you call too early?");
        }
        return _instance;
    }

    void MpStatusRepository::ReportStatus(MultiplayerCore::Models::MpStatusData* statusData) {
        _lastStatus = statusData;
        _statusByUrl->set_Item(StringW(MultiplayerCore::Hooks::NetworkConfigHooks::GetCurrentServer()->masterServerStatusUrl), statusData);
        statusUpdatedForUrlEvent.invoke(StringW(MultiplayerCore::Hooks::NetworkConfigHooks::GetCurrentServer()->masterServerStatusUrl), statusData);
    }

    MultiplayerCore::Models::MpStatusData* MpStatusRepository::GetLastStatusData() {
        return _lastStatus;
    }
}

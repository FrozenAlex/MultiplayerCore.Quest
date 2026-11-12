#include "Networking/MultiplayerTypes.hpp"
#include "ScoreSyncState/MpScoreSyncStateManager.hpp"

DEFINE_TYPE(MultiplayerCore::ScoreSyncState, MpScoreSyncStateManager);

namespace MultiplayerCore::ScoreSyncState {
    void MpScoreSyncStateManager::ctor() {
        INVOKE_CTOR();
        INVOKE_BASE_CTOR(i2c::class_of<GlobalNamespace::ScoreSyncStateManager*>());
        deltaUpdateFrequencyMs = 10;
        fullStateUpdateFrequencyMs = 100;
    }

    void MpScoreSyncStateManager::Inject(Networking::MpPacketSerializer* packetSerializer) {
        _packetSerializer = packetSerializer;
    }

    void MpScoreSyncStateManager::Initialize() {
        _packetSerializer->RegisterCallback<MpScoreSyncStatePacket*>(
            std::bind(&MpScoreSyncStateManager::HandleScoreSyncUpdateReceived, this, std::placeholders::_1, std::placeholders::_2)
        );
    }

    void MpScoreSyncStateManager::Dispose() {
        _packetSerializer->UnregisterCallback<MpScoreSyncStatePacket*>();
    }

    void MpScoreSyncStateManager::HandleScoreSyncUpdateReceived(MpScoreSyncStatePacket* data, GlobalNamespace::IBeatSaberConnectedPlayer* player) {
        if (MultiplayerCore::ConnectedPlayer(player)->get_isConnectionOwner()) {
            DEBUG("Updating score sync frequency to following values: delta: {}ms, full: {}ms", data->deltaUpdateFrequencyMs, data->fullStateUpdateFrequencyMs);
            bool shouldForceUpdate = deltaUpdateFrequencyMs != data->deltaUpdateFrequencyMs || fullStateUpdateFrequencyMs != data->fullStateUpdateFrequencyMs;
            deltaUpdateFrequencyMs = data->deltaUpdateFrequencyMs;
            fullStateUpdateFrequencyMs= data->fullStateUpdateFrequencyMs;
            if (shouldForceUpdate) {
                DEBUG("Forcing new state buffer update");
                _localState = nullptr;
            }
        }
    }

    long MpScoreSyncStateManager::get_deltaUpdateFrequencyMs() { 
        DEBUG("Returning delta update frequency: {}ms", deltaUpdateFrequencyMs);
        return deltaUpdateFrequencyMs; 
    }
    long MpScoreSyncStateManager::get_fullStateUpdateFrequencyMs() { 
        DEBUG("Returning full state update frequency: {}ms", fullStateUpdateFrequencyMs);
        return fullStateUpdateFrequencyMs; 
    }
}

#include "Networking/MultiplayerTypes.hpp"
#include "Networking/MpPacketSerializer.hpp"
#include "logging.hpp"

#include "System/Type.hpp"
DEFINE_TYPE(MultiplayerCore::Networking, MpPacketSerializer);

namespace MultiplayerCore::Networking {
    MpPacketSerializer::ProcessedHandler* MpPacketSerializer::get_PacketProcessedHandler() { return _packetProcessedHandler; }
    void MpPacketSerializer::set_PacketProcessedHandler(ProcessedHandler* value) { _packetProcessedHandler = value; }
    void MpPacketSerializer::ctor(GlobalNamespace::IBeatSaberMultiplayerSessionManager* sessionManager) {
        INVOKE_CTOR();
        _sessionManager = i2c::try_cast<GlobalNamespace::BeatSaberMultiplayerSessionManager*>(sessionManager);
    }

    void MpPacketSerializer::Initialize() {
        _sessionManager->RegisterSerializer(GlobalNamespace::NetworkMessageType(ID), this->i_INetworkPacketSubSerializer_1_IConnectedPlayer());
    }

    void MpPacketSerializer::Dispose() {
        try {
            _sessionManager->UnregisterSerializer(GlobalNamespace::NetworkMessageType(ID), this->i_INetworkPacketSubSerializer_1_IConnectedPlayer());
        }
        catch (const std::exception& e) {
            ERROR("Failed to unregister serializer: {}", e.what());
        }
    }

    void MpPacketSerializer::Serialize(LiteNetLib::Utils::NetDataWriter* writer, LiteNetLib::Utils::INetSerializable* packet) {
        auto packetType = reinterpret_cast<System::Object*>(packet)->GetType();
        writer->Put(packetType->NameOrDefault);
        packet->Serialize(writer);
    }

    void MpPacketSerializer::Deserialize(LiteNetLib::Utils::NetDataReader* reader, int length, GlobalNamespace::IBeatSaberConnectedPlayer* data) {
        std::string packetId = "null";
        auto prevPosition = reader->get_Position();
        try {
            packetId = static_cast<std::string>(reader->GetString());
            length -= reader->get_Position() - prevPosition;
            prevPosition = reader->get_Position();

            auto handlerItr = packetHandlers.find(packetId);
            if (handlerItr != packetHandlers.end() && handlerItr->second) {
                handlerItr->second(reader, length, data);
                if (_packetProcessedHandler) _packetProcessedHandler->HandlePacketProcessed(packetId, data);
            }
        }
        catch (const std::exception& e) {
            std::string user;
			if (data) {
				if (MultiplayerCore::ConnectedPlayer(data)->get_userName())
					user += static_cast<std::string>(MultiplayerCore::ConnectedPlayer(data)->get_userName());
				if (!user.empty() && MultiplayerCore::ConnectedPlayer(data)->get_userId()) user += "|";
				if (MultiplayerCore::ConnectedPlayer(data)->get_userId())
					user += static_cast<std::string>(MultiplayerCore::ConnectedPlayer(data)->get_userId());
			}
            WARNING("An exception was thrown processing packet from player {}, packet {}: {}", user, packetId, e.what());
        }
        int processedBytes = reader->get_Position() - prevPosition;
        reader->SkipBytes(length - processedBytes);
    }

    bool MpPacketSerializer::HandlesType(System::Type* type) {
        return std::find(registeredTypes.begin(), registeredTypes.end(), type) != registeredTypes.end();
    }

    GlobalNamespace::IBeatSaberConnectedPlayer* MpPacketSerializer::get_connectionOwner() {
        return _sessionManager->get_connectionOwner();
    }
}

#include "MultiplayerTypes.hpp"
#pragma once

#include "System/Object.hpp"
#include "custom-types/shared/macros.hpp"
#if __has_include("logging.hpp")
#include "logging.hpp"
#endif

#include "GlobalNamespace/INetworkPacketSubSerializer_1.hpp"
#include "GlobalNamespace/INetworkPacketProcessor_1.hpp"
#include "GlobalNamespace/INetworkPacketProcessedHandler_1.hpp"
#include "GlobalNamespace/NetworkMessageType.hpp"
#include "MultiplayerTypes.hpp"
#include "GlobalNamespace/IBeatSaberConnectedPlayer.hpp"
#include "LiteNetLib/Utils/NetDataWriter.hpp"
#include "LiteNetLib/Utils/NetDataReader.hpp"
#include "LiteNetLib/Utils/INetSerializable.hpp"
#include "GlobalNamespace/IPoolablePacket.hpp"

#include "System/IDisposable.hpp"
#include "System/Type.hpp"
#include "Zenject/IInitializable.hpp"

#include "GlobalNamespace/BeatSaberMultiplayerSessionManager.hpp"
#include "GlobalNamespace/ThreadStaticPacketPool_1.hpp"

#include <type_traits>
#include <map>
#include <list>

namespace MultiplayerCore {
    template<typename TPacket>
    concept INetSerializable = requires(TPacket t) {
        {t->i_INetSerializable()} -> std::same_as<LiteNetLib::Utils::INetSerializable*>;
    };

    template<typename TPacket>
    concept IPoolablePacket = INetSerializable<TPacket> && requires(TPacket t) {
        {t->i_IPoolablePacket()} -> std::same_as<GlobalNamespace::IPoolablePacket*>;
        {GlobalNamespace::ThreadStaticPacketPool_1<TPacket>::get_pool()->Obtain()} -> std::same_as<TPacket>;
    };
}

using PacketHandler = std::function<void (LiteNetLib::Utils::NetDataReader*, int, GlobalNamespace::IBeatSaberConnectedPlayer*)>;

DECLARE_CLASS_CODEGEN_INTERFACES(MultiplayerCore::Networking, MpPacketSerializer, System::Object,
    GlobalNamespace::INetworkPacketSubSerializer_1<GlobalNamespace::IBeatSaberConnectedPlayer*>*, GlobalNamespace::INetworkPacketProcessor_1<GlobalNamespace::IBeatSaberConnectedPlayer*>*, ::Zenject::IInitializable*, System::IDisposable*) {
        
    DECLARE_OVERRIDE_METHOD_MATCH(void, Initialize, &::Zenject::IInitializable::Initialize);
    DECLARE_OVERRIDE_METHOD_MATCH(void, Dispose, &::System::IDisposable::Dispose);
    DECLARE_OVERRIDE_METHOD(void, Serialize, (i2c::find_method(i2c::class_of<GlobalNamespace::INetworkPacketSubSerializer_1<GlobalNamespace::IBeatSaberConnectedPlayer*>*>(), {"Serialize", 2})), LiteNetLib::Utils::NetDataWriter* writer, LiteNetLib::Utils::INetSerializable* packet);
    DECLARE_OVERRIDE_METHOD(void, Deserialize, (i2c::find_method(i2c::class_of<GlobalNamespace::INetworkPacketSubSerializer_1<GlobalNamespace::IBeatSaberConnectedPlayer*>*>(), {"Deserialize", 3})), LiteNetLib::Utils::NetDataReader* reader, int length, GlobalNamespace::IBeatSaberConnectedPlayer* data);
    DECLARE_OVERRIDE_METHOD(bool, HandlesType, (i2c::find_method(i2c::class_of<GlobalNamespace::INetworkPacketSubSerializer_1<GlobalNamespace::IBeatSaberConnectedPlayer*>*>(), {"HandlesType", 1})), System::Type* type);

    static const uint8_t ID = 100u;

    using ProcessedHandler = GlobalNamespace::INetworkPacketProcessedHandler_1<GlobalNamespace::IBeatSaberConnectedPlayer*>;
    using Processor = GlobalNamespace::INetworkPacketProcessor_1<GlobalNamespace::IBeatSaberConnectedPlayer*>;
    DECLARE_INSTANCE_FIELD_PRIVATE(ProcessedHandler*, _packetProcessedHandler);
    DECLARE_OVERRIDE_METHOD(ProcessedHandler*, get_PacketProcessedHandler, (i2c::find_method(i2c::class_of<Processor*>(), {"get_PacketProcessedHandler", 0})));
    DECLARE_OVERRIDE_METHOD(void, set_PacketProcessedHandler, (i2c::find_method(i2c::class_of<Processor*>(), {"set_PacketProcessedHandler", 1})), ProcessedHandler* value);

    DECLARE_INSTANCE_FIELD_PRIVATE(GlobalNamespace::BeatSaberMultiplayerSessionManager*, _sessionManager);

    DECLARE_CTOR(ctor, GlobalNamespace::IBeatSaberMultiplayerSessionManager* sessionManager);

    public:
        GlobalNamespace::INetworkPacketSubSerializer_1<GlobalNamespace::IBeatSaberConnectedPlayer*>* i_INetworkPacketSubSerializer_1_IConnectedPlayer() { return reinterpret_cast<GlobalNamespace::INetworkPacketSubSerializer_1<GlobalNamespace::IBeatSaberConnectedPlayer*>*>(this); }
        Zenject::IInitializable* i_IInitializable() { return reinterpret_cast<::Zenject::IInitializable*>(this); }
        System::IDisposable* i_IDisposable() { return reinterpret_cast<::System::IDisposable*>(this); }

        template<::MultiplayerCore::IPoolablePacket TPacket>
        requires(std::is_pointer_v<TPacket>)
        static TPacket ObtainPacket() {
            return GlobalNamespace::ThreadStaticPacketPool_1<TPacket>::get_pool()->Obtain();
        }

        template<::MultiplayerCore::INetSerializable TPacket>
        requires(std::is_pointer_v<TPacket> && !::MultiplayerCore::IPoolablePacket<TPacket>)
        static TPacket ObtainPacket() {
            return std::remove_pointer_t<TPacket>::New_ctor();
        }

        template<::MultiplayerCore::INetSerializable TPacket>
        requires(std::is_pointer_v<TPacket>)
        static TPacket DeserializePacket(LiteNetLib::Utils::NetDataReader* reader, int size) {
            auto packet = ObtainPacket<TPacket>();
            if (!packet) {
                reader->SkipBytes(size);
            } else packet->Deserialize(reader);
            return packet;
        }

        template<::MultiplayerCore::INetSerializable TPacket>
        requires(std::is_pointer_v<TPacket>)
        void RegisterCallback(std::function<void(TPacket)> callback) { RegisterCallback([callback](TPacket packet, GlobalNamespace::IBeatSaberConnectedPlayer* player){ callback(packet); }); }

        template<::MultiplayerCore::INetSerializable TPacket>
        requires(std::is_pointer_v<TPacket>)
        void RegisterCallback(std::function<void(TPacket, GlobalNamespace::IBeatSaberConnectedPlayer*)> callback) {
            System::Type* packetType = i2c::cs_type_of<TPacket>();
            registeredTypes.emplace_back(packetType);
            std::string packetId(packetType->NameOrDefault);

            packetHandlers[packetId] = [callback](LiteNetLib::Utils::NetDataReader* reader, int size, GlobalNamespace::IBeatSaberConnectedPlayer* player){
                callback(DeserializePacket<TPacket>(reader, size), player);
            };
        }

        template<::MultiplayerCore::INetSerializable TPacket>
        requires(std::is_pointer_v<TPacket>)
        void RegisterCallback(const std::string& packetId, std::function<void(TPacket)> callback) { RegisterCallback(packetId, [callback](TPacket packet, GlobalNamespace::IBeatSaberConnectedPlayer* player){ callback(packet); }); }

        template<::MultiplayerCore::INetSerializable TPacket>
        requires(std::is_pointer_v<TPacket>)
        void RegisterCallback(const std::string& packetId, std::function<void(TPacket, GlobalNamespace::IBeatSaberConnectedPlayer*)> callback) {
            System::Type* packetType = i2c::cs_type_of<TPacket>();
            registeredTypes.emplace_back(packetType);

            packetHandlers[packetId] = [callback](LiteNetLib::Utils::NetDataReader* reader, int size, GlobalNamespace::IBeatSaberConnectedPlayer* player){
                callback(DeserializePacket<TPacket>(reader, size), player);
            };
        }

        template<::MultiplayerCore::INetSerializable TPacket>
        requires(std::is_pointer_v<TPacket>)
        void UnregisterCallback() {
            System::Type* packetType = i2c::cs_type_of<TPacket>();
            std::string packetId(packetType->NameOrDefault);

            std::erase(registeredTypes, packetType);
            packetHandlers.erase(packetId);
        }

        /// <summary>
        /// Obtains the connection owner / the player representing the dedicated server
        /// </summary>
        GlobalNamespace::IBeatSaberConnectedPlayer* get_connectionOwner();
        __declspec(property(get = get_connectionOwner))::GlobalNamespace::IBeatSaberConnectedPlayer* connectionOwner;

        /// <summary>
        /// Sends a packet to the server / connection owner
        /// </summary>
        template<::MultiplayerCore::INetSerializable TPacket>
        requires(std::is_pointer_v<TPacket>)
        void SendToServer(TPacket packet) {
            /* TODO: try catch, logging? */
            if (_sessionManager) {
                _sessionManager->SendToPlayer(packet->i_INetSerializable(), MultiplayerCore::ConnectedPlayer(connectionOwner));
            }
        }

        template<::MultiplayerCore::INetSerializable TPacket>
        requires(std::is_pointer_v<TPacket>)
        void SendToPlayer(TPacket packet, GlobalNamespace::IBeatSaberConnectedPlayer* player) {
            /* TODO: try catch, logging? */
            if (_sessionManager) {
                _sessionManager->SendToPlayer(packet->i_INetSerializable(), MultiplayerCore::ConnectedPlayer(player));
            }
        }


        template<::MultiplayerCore::INetSerializable TPacket>
        requires(std::is_pointer_v<TPacket>)
        void Send(TPacket packet) {
            /* TODO: try catch, logging? */
            if (_sessionManager) {
                _sessionManager->Send(packet->i_INetSerializable());
            }
        }

        template<::MultiplayerCore::INetSerializable TPacket>
        requires(std::is_pointer_v<TPacket>)
        void SendUnreliable(TPacket packet) {
            /* TODO: try catch, logging? */
            if (_sessionManager) {
                _sessionManager->SendUnreliable(packet->i_INetSerializable());
            }
        }

    private:
        std::list<System::Type*> registeredTypes;
        std::map<std::string, PacketHandler> packetHandlers;

};

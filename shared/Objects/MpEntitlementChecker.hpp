#include "../Networking/MultiplayerTypes.hpp"
#pragma once

#include "beatsaber-hook/shared/safeptr.hpp"
#include "beatsaber-hook/shared/callback.hpp"
#include "System/Object.hpp"
#include "custom-types/shared/macros.hpp"
#include "lapiz/shared/macros.hpp"
#include "GlobalNamespace/NetworkPlayerEntitlementChecker.hpp"
#include "GlobalNamespace/IBeatSaberMultiplayerSessionManager.hpp"
#include "GlobalNamespace/EntitlementsStatus.hpp"
#include "System/Collections/Concurrent/ConcurrentDictionary_2.hpp"
#include "System/Threading/Tasks/Task_1.hpp"
#include "System/Threading/Tasks/TaskCompletionSource_1.hpp"

#include <future>
#include <unordered_map>
#include <string>
#include <mutex>
#include <atomic>
#include "System/Action_3.hpp"
using EntitlementAction = System::Action_3<StringW, StringW, GlobalNamespace::EntitlementsStatus>;

using EntitlementsStatusTask = ::System::Threading::Tasks::Task_1<::GlobalNamespace::EntitlementsStatus>;

DECLARE_CLASS_CODEGEN(MultiplayerCore::Objects, MpEntitlementChecker, GlobalNamespace::NetworkPlayerEntitlementChecker) {
    DECLARE_INSTANCE_FIELD_PRIVATE(MultiplayerCore::SessionManager*, _sessionManager);
    DECLARE_INSTANCE_FIELD_PRIVATE(EntitlementAction*, _entitlementDelegate);

    DECLARE_INJECT_METHOD(void, Inject, GlobalNamespace::IBeatSaberMultiplayerSessionManager* sessionManager);

    DECLARE_INSTANCE_METHOD(void, Start_override);
    DECLARE_INSTANCE_METHOD(void, OnDestroy_override);
    DECLARE_INSTANCE_METHOD(void, HandleGetIsEntitledToLevel_override, StringW userId, StringW levelId);
    DECLARE_INSTANCE_METHOD(EntitlementsStatusTask*, GetEntitlementStatus_override, StringW levelId);

    DECLARE_INSTANCE_METHOD(void, HandleSetIsEntitledToLevel, StringW userId, StringW levelId, GlobalNamespace::EntitlementsStatus entitlement);
    DECLARE_INSTANCE_METHOD(GlobalNamespace::EntitlementsStatus, GetKnownEntitlement, StringW userId, StringW levelId);

    DECLARE_CTOR(ctor);
    DECLARE_SIMPLE_DTOR();
    public:
        unordered_event_callback<std::string, std::string, GlobalNamespace::EntitlementsStatus> receivedEntitlementEvent;

    private:
        std::mutex _entitlementsMutex;
        std::atomic<bool> _destroyed{false};
        GlobalNamespace::EntitlementsStatus GetEntitlementStatus(std::string_view levelHash);
        std::unordered_map<std::string, std::unordered_map<std::string, GlobalNamespace::EntitlementsStatus>> _entitlementsDictionary;
        std::unordered_map<std::string, safe_ptr<EntitlementsStatusTask*>> _entitlementsTasks;
};

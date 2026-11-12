#include "hooking.hpp"
#include "Hooks/NetworkConfigHooks.hpp"
#include "Models/MpStatusData.hpp"
#include "Newtonsoft/Json/JsonConvert.hpp"
#include "UnityEngine/JsonUtility.hpp"
#include "System/Type.hpp"
// Non-generic entry points avoid shared generic-method metadata/ABI assumptions.
MAKE_AUTO_HOOK_ORIG_MATCH(JsonConvert_DeserializeObject_Status, &Newtonsoft::Json::JsonConvert::DeserializeObject,
    System::Object*, StringW json, System::Type* type, Newtonsoft::Json::JsonSerializerSettings* settings) {
    if (MultiplayerCore::Hooks::NetworkConfigHooks::IsOverridingAPI() && type == i2c::cs_type_of<GlobalNamespace::MultiplayerStatusData*>())
        return MultiplayerCore::Models::MpStatusData::New_ctor(json);
    return JsonConvert_DeserializeObject_Status(json, type, settings);
}
MAKE_AUTO_HOOK_ORIG_MATCH(JsonUtility_FromJson_Status, &UnityEngine::JsonUtility::FromJson,
    System::Object*, StringW json, System::Type* type) {
    if (MultiplayerCore::Hooks::NetworkConfigHooks::IsOverridingAPI() && type == i2c::cs_type_of<GlobalNamespace::MultiplayerStatusData*>())
        return MultiplayerCore::Models::MpStatusData::New_ctor(json);
    return JsonUtility_FromJson_Status(json, type);
}

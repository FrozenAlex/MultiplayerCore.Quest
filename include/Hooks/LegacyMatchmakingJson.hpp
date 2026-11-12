#pragma once
#include <string>
#include <stdexcept>
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/document.h"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/stringbuffer.h"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/writer.h"

namespace MultiplayerCore::Hooks {
// Kept independent of IL2CPP so the legacy master contract can be checked on the host.
inline std::string AddLegacyCredentials(const std::string& body, const std::string& userId, const std::string& proof) {
    if (userId.empty() || proof.empty()) throw std::invalid_argument("Missing multiplayer credentials");
    rapidjson::Document document;
    document.Parse(body.c_str(), body.size());
    if (document.HasParseError() || !document.IsObject()) throw std::invalid_argument("Invalid matchmaking request JSON");
    auto& allocator = document.GetAllocator();
    while (document.HasMember("auth_user_id")) document.RemoveMember("auth_user_id");
    while (document.HasMember("single_use_auth_token")) document.RemoveMember("single_use_auth_token");
    document.AddMember("auth_user_id", rapidjson::Value(userId.data(), userId.size(), allocator), allocator);
    document.AddMember("single_use_auth_token", rapidjson::Value(proof.data(), proof.size(), allocator), allocator);
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);
    return {buffer.GetString(), buffer.GetSize()};
}
}

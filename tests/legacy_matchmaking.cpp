#include "Hooks/LegacyMatchmakingJson.hpp"
#include <cassert>
#include <iostream>

using MultiplayerCore::Hooks::AddLegacyCredentials;
int main() {
    // Preserve the game's routing, ticket, and configuration data verbatim in meaning.
    const std::string body = R"({"user_id":"hash","code":"ROOM","ticket":null,"configuration":{"max_player_count":5},"auth_user_id":"old","single_use_auth_token":"old"})";
    const auto result = AddLegacyCredentials(body, "1234567890", "proof\"with\\escapes");
    rapidjson::Document data;
    data.Parse(result.c_str());
    assert(!data.HasParseError());
    assert(std::string(data["user_id"].GetString()) == "hash");
    assert(std::string(data["code"].GetString()) == "ROOM");
    assert(data["ticket"].IsNull());
    assert(data["configuration"]["max_player_count"].GetInt() == 5);
    assert(std::string(data["auth_user_id"].GetString()) == "1234567890");
    assert(std::string(data["single_use_auth_token"].GetString()) == "proof\"with\\escapes");
    auto duplicate = AddLegacyCredentials(R"({"auth_user_id":"a","auth_user_id":"b","single_use_auth_token":"a","single_use_auth_token":"b"})", "id", "proof");
    data.Parse(duplicate.c_str());
    assert(data.MemberCount() == 2);
    for (const auto& invalid : {"[]", "null", "{", "{\"a\":1} trailing"}) {
        bool threw = false;
        try { AddLegacyCredentials(invalid, "id", "proof"); } catch (const std::invalid_argument&) { threw = true; }
        assert(threw);
    }
    for (bool missingUser : {false, true}) {
        bool threw = false;
        try { AddLegacyCredentials("{}", missingUser ? "" : "id", missingUser ? "proof" : ""); }
        catch (const std::invalid_argument&) { threw = true; }
        assert(threw);
    }
    std::cout << "Legacy matchmaking contract checks passed\n";
}

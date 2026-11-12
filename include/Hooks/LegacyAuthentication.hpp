#pragma once
#include <string>
namespace MultiplayerCore::Hooks {
struct LegacyCredentials { std::string graphUrl, userId, proof; };
LegacyCredentials GetLegacyCredentials();
}

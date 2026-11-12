#include "hooking.hpp"
#include "logging.hpp"
#include "GlobalNamespace/MultiplayerPlayersManager.hpp"
#include "GlobalNamespace/MultiplayerLobbyCenterStageManager.hpp"
#include "GlobalNamespace/MultiplayerLobbyAvatarPlaceManager.hpp"
#include "GlobalNamespace/MultiplayerLobbyAvatarManager.hpp"
#include "GlobalNamespace/MultiplayerPlayerPlacement.hpp"
#include <sys/mman.h>
#include <unistd.h>
#include <cstdint>

namespace {
// Verified against the unmodified Quest 1.45.3_30054 libil2cpp.so.
// Check the instruction AND its consumer, and refuse mismatching builds.
// Evidence: profiling/multiplayer-1.40.8-to-1.45.3-20261112/native/new/*.patch-review.txt
void PatchCount(const MethodInfo* method, size_t offset, uint32_t expected, uint32_t replacement) {
    auto instruction = reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(method->methodPointer) + offset);
    if (instruction[0] != expected || instruction[1] != 0x1e220100u) { // scvtf s0,w8
        ERROR("Skipping incompatible player-placement patch: {}", method->name);
        return;
    }
    const auto pageSize = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
    if (!pageSize || pageSize == static_cast<uintptr_t>(-1)) return;
    auto page = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(instruction) & ~(pageSize - 1));
    if (mprotect(page, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC)) {
        ERROR("Could not make player-placement instruction writable: {}", method->name);
        return;
    }
    *instruction = replacement;
    __builtin___clear_cache(reinterpret_cast<char*>(instruction), reinterpret_cast<char*>(instruction + 1));
    if (mprotect(page, pageSize, PROT_READ | PROT_EXEC))
        ERROR("Could not restore code-page permissions: {}", method->name);
}
}
void Patch_EvenPlayerPlacement1453() {
    // The compiler inlined the placement helper into SpawnPlayers.
    PatchCount(i2c::metadata_getter<&GlobalNamespace::MultiplayerPlayersManager::SpawnPlayers>::method_info(),
               0x57c, 0x1a800408u, 0x2a0003e8u); // cinc w8,w0,ne -> mov w8,w0
    PatchCount(i2c::metadata_getter<&GlobalNamespace::MultiplayerLobbyCenterStageManager::RecalculateCenterPosition>::method_info(),
               0xb8, 0x32000108u, 0xd503201fu); // orr w8,w8,#1 -> nop
    PatchCount(i2c::metadata_getter<&GlobalNamespace::MultiplayerLobbyAvatarPlaceManager::SpawnAllPlaces>::method_info(),
               0x10c, 0x32000108u, 0xd503201fu);
    PatchCount(i2c::metadata_getter<&GlobalNamespace::MultiplayerLobbyAvatarManager::AddPlayer>::method_info(),
               0x270, 0x32000108u, 0xd503201fu);
}
AUTO_INSTALL_PATCH(EvenPlayerPlacement1453);
MAKE_AUTO_HOOK_ORIG_MATCH(MultiplayerPlayerPlacement_GetAngle,
    &GlobalNamespace::MultiplayerPlayerPlacement::GetAngleBetweenPlayersWithEvenAdjustment,
    float, int numberOfPlayers, GlobalNamespace::MultiplayerPlayerLayout layout) {
    return numberOfPlayers > 0 ? 360.0f / numberOfPlayers : MultiplayerPlayerPlacement_GetAngle(numberOfPlayers, layout);
}

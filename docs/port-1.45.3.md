## Published branch status — 12 November 2026

This `1.45.3` branch targets Beat Saber **1.45.3_30054**. PC and Quest have
been tested together on the updated LAN UnifiedServer: both join a lobby,
start a Standard custom song, and show the failed screen. The user also tested
Quest MultiplayerExtensions. Full-song completion, late joins, reconnects and
all per-player options are not individually verified.

Runtime fixes include PC menu activation, Quest network configuration setup,
Quest platform identity injection, nonvirtual beatmap-view base calls, dedicated
server state flags, and a results window when all players fail. Validation passed:
19 dedicated-server tests, 10 HTTP tests, 7 PC tests, PC hook/signature audits,
Quest ARM64 builds, QMOD checks, and installed-library hash/load checks.

The PC BSML, SiraUtil and SongCore dependency ports are intentionally **not
published** with these branches. Reproducing the PC test requires compatible
versions of those dependencies. Quest's core pack is likewise separate; use the
versions declared by each QMOD/QPM manifest. Public BeatTogether service and
external credential verification were not validated: the LAN server disabled
authentication for the test. No general production-readiness claim is made.

The notes below preserve the port investigation/build history. Statements there
about pending tests or uncommitted changes describe earlier stages; the verified
scope above is current. The combined repository/build guide is available at
https://github.com/FrozenAlex/BeatTogether.UnifiedServer/blob/1.45.3/docs/crossplay-1.45.3.md.

---

# Quest 1.45.3 port

Local test version **1.6.8**, targeting **1.45.3_30054**. Runtime validation is
pending. This package must be used with a compatible dedicated server; the
inspected game's compatibility identifier is `A31AF687` (transport protocol 9).
It does not translate 1.40.8 clients into the new protocol.

## Changes

- Uses `IBeatSaberMultiplayerSessionManager`, its generic session interface and
  `IBeatSaberConnectedPlayer`; common player fields go through `IConnectedPlayer`.
- Implements the packet-processed callback required by the new serializer API.
  MultiplayerCore's custom message ID remains 100 and retains type-name framing.
- Ports beatmap characteristic/gameplay keys, level loading, spectator scene
  transitions, player metadata, status parsing and server configuration hooks.
- Gets a real Oculus user proof for custom-server matchmaking, with cancellation
  and a ten-second timeout. The HTTP bridge adds `auth_user_id` and
  `single_use_auth_token` to the new game's JSON body, retaining its other fields.
  It uses the individual request's proof and checks the selected endpoint. Failed
  proof retrieval fails authentication. Official servers use the game's token path.
- Updates four even-player lobby patches using exact 1.45.3 ARM64 instructions,
  guards their expected opcodes, uses the system page size and restores RX access.
- Roots tasks/owners during native asynchronous work, synchronizes download and
  BeatSaver cache results, removes the entitlement event subscription on teardown,
  and moves preview Unity asset/network operations onto the main thread. Preview
  generations prevent late downloads repopulating an unloaded preview.
- Uses hook 9, cordl 4503, NDK 29, C++23 and the GameActivity launch name.

The dedicated-server counterpart is based on BeatTogether's `dev-1.42.0` at
`754a67c86debb617748a3a228552092742e319eb`. It adds the compatibility strings,
active-hand identity field/cache and game-specific avatar/controller dispatch.

## Build

Resolved dependencies are recorded in `qpm.shared.json`: hook 9.4.3, cordl
4503.0.0, custom-types 0.21.2, Lapiz 0.3.7, BSML 0.6.7, SongCore 1.3.5,
Paper 4.8.0 and Flamingo 1.3.1. Use QPM and Android NDK 29 with C++23 support.

```powershell
qpm restore
qpm s build
qpm qmod manifest
qpm qmod zip --skip_build

# Required before restoring the two dependent Quest ports:
qpm install --cmake-build false build/libMultiplayerCore.so build/debug/libMultiplayerCore.so
```

`qpm install` here updates the local QPM cache; it is not a remote publication.
These local versions have no published release URLs. Install the supplied core
QMOD before BeatTogether 2.2.6 and MultiplayerExtensions 1.1.2. Their manifests
require MultiplayerCore 1.6.8.

The portable matchmaking contract checks can be run with host Clang:

```powershell
clang++ -std=c++20 -Iinclude -Iextern/includes tests/legacy_matchmaking.cpp -o build/legacy_matchmaking_tests.exe
./build/legacy_matchmaking_tests.exe
```

They cover preserved request data, credential replacement, escaping, duplicate
credential fields, malformed JSON and missing credentials. They do not exercise
the Oculus SDK or actual authentication against the master.

## Runtime acceptance

Use two Quest clients on 1.45.3 and a staging master that routes that game version
to the updated dedicated-server pool. Verify create/join, authentication failure,
late join, avatar and active-hand changes, custom-song recommendation/download,
download cancellation/failure, PPD/PPM, countdown, full/delta pose and score state,
results, spectator entry and disconnect/rejoin. Repeat leaving the lobby while a
preview or song download is pending. Test MultiplayerExtensions' solo environment
after the basic session works.

No headset, live ENet session, production master, older-client translation,
PC crossplay or Pico authentication has been validated by this port's host checks.
The supporting PC decompilation was 1.45.2; the protocol and instruction checks
used the exact Quest 1.45.3 APK.

## Installed Quest startup follow-up (12 November)

The first device run exposed an early `INetworkConfig` resolve in
`MainSystemInit.InstallBindings`. Network config initialization now hooks
`MainSettingsAsyncLoader.RegisterInstallers` before the settings installer is
registered, using the loader's existing config asset. The subsequent run exposed
obsolete `UserInfo` constructor injection in `MpPlayerManager`; it now injects
`IPlatform` and builds its local identity from that service during initialization.
Both changes preserve the existing custom server selection and packet format.

NDK 29 builds and portable legacy matchmaking contract checks pass. The rebuilt
QMOD is installed for the LAN crossplay test. Evidence, prior binaries and live
validation status are under
`../build-pc-dependencies-1.45.3/quest-crossplay-20261112/`.
The earlier host-only runtime limitations above describe the pre-install state;
a two-client gameplay pass is still required.

### First Quest lobby attempt

The Quest successfully created and connected to the local ENet server over Wi-Fi
on UDP 31001. Lobby UI initialization then overflowed the native stack in
`CustomEditableBeatmapSelectionView.SetBeatmap`: qualifying a generated C++ wrapper
with its base class still performs managed virtual dispatch. Both custom beatmap
selection views now invoke the base `MethodInfo` directly. This preserves empty
and official-map behavior without re-entering the override.

MultiplayerExtensions was rebuilt against the new `MpPlayerManager` constructor
signature; its old build referenced the removed UserInfo signature and did not
load after that API change. All MultiplayerCore native imports are checked against
the rebuilt library before deploying the complete set. The local QPM cache was
refreshed for this dependency rebuild; nothing was published remotely.

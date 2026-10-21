# TES3MP on OpenMW 0.52: multiplayer status

**Audit date:** 2026-10-20
**Branch:** `rebase-0.52-merge`
**Audited commit:** `5a9214425c` (`Complete TES3MP rebase onto OpenMW 0.52`)

## Bottom line

The rebase builds, and the OpenMW/components unit suites pass. The branch also
contains substantial TES3MP networking, packet, object, player, actor, and
world-state code. That is **not evidence that OpenMW 0.52 is fully playable in
multiplayer**. The 0.52 mechanics refactor left core synchronization gaps in
AI/authority and melee combat. Two clients now pass pre-init, log in, and create
remote player replicas without the earlier `RefId` crash. A server-driven
zero-health test exercised death notification and resurrection, and a
server-injected summon packet was reconstructed on both clients. Normal player
movement packets and orderly client disconnects are now verified. Combat and a
player-cast summon remain unverified.

This report separates **built/tested**, **implemented in source**, and
**known incomplete**. “Implemented in source” means that send/receive code is
present; it does not mean that the behavior was exercised successfully in a
real session.

## Validation performed

- Full configured build completed, producing `tes3mp`, `tes3mp-server`, and
  `openmw-lib`.
- The optional `masterserver` target also built.
- `components-tests`: **1,596 passed**.
- `openmw-tests`: **529 passed**.
- A local server startup smoke test loaded an isolated config, bound to
  loopback with master-server registration disabled, and shut down cleanly on
  SIGINT. This did not connect a client.
- ESMTool parsed `Morrowind.esm`, `Tribunal.esm`, and `Bloodmoon.esm`.
- With exact CRC requirements configured for `builtin.omwscripts` and all
  three ESMs, two concurrent clients passed server pre-init, loaded the ESMs
  and BSA archives, started a new game, and sent the starting map tile. The
  server logged the expected CRCs (`025FFEE8`, `7B6AF5B9`, `F481F334`,
  `43DD2132`) and accepted both clients.
- A single client was driven through name entry. The server received base info
  and a cell change, fired `OnPlayerConnect`, and completed the cell-change
  handler without crashing.
- Two clients logged in and each created a `DedicatedPlayer` replica of the
  other. A raw cross-process `ESM::RefId` in base-info initially crashed the
  receiving client; after changing packet `RefId` serialization to text, the
  same two-client login and remote-replica creation completed without that
  crash.
- A test plugin changed one player's server-side position and exterior cell.
  The client received `ID_PLAYER_POSITION` and `ID_PLAYER_CELL_CHANGE`, then
  reported the cell change back to the server. This tests the scripted
  position/cell synchronization path, not walking across a boundary.
- With the movement sender fix in place, focused W-key input moved a client in
  the world. The server's position-poll callback observed several changing XYZ
  values; this verifies player position transmission, but not another cell
  boundary or remote interpolation accuracy.
- With two remote player replicas present, both clients received SIGTERM and
  exited with status 0; the server handled both disconnects and then shut down
  cleanly. Earlier versions crashed while destroying remote player state.
- The test plugin set a player's health to zero and sent dynamic stats. The
  client emitted exactly one `ID_PLAYER_DEATH`; the server handled it and fired
  `OnPlayerDeath`. A server-sent regular resurrection was received by the
  client. This confirms the death echo no longer loops back into a second
  outbound packet.
- A server script sent a summon-aware `ID_OBJECT_SPAWN` for a Scamp to both
  clients. Both reconstructed the actor and its summon effect/master data.
  This verifies the receiver path, not a spell cast or the client-originated
  summon hook.
- A server script granted and equipped an iron dagger on a client; the client
  applied the inventory and equipment packets. Player-to-player item transfer
  was not tested.
- `tes3mp --help` and `tes3mp --version` ran; the version reports OpenMW
  0.52.0 at this commit.
- `ctest --test-dir build -N` reports **zero registered tests**. The two
  GoogleTest executables above were run directly.

The unit tests are primarily OpenMW/components tests, not TES3MP protocol or
gameplay tests. The supplied installation is available at `/mnt/Morrowind`;
the clients used the build's resource VFS and that installation's data files.
Xvfb allowed rendering and server/client integration checks. Focused W-key input
produced position updates, while automated attack/cast input remained
inconclusive.

## Multiplayer paths present in the source

| Area | Source implementation | Assessment |
|---|---|---|
| Connection and compatibility checks | Client `mwmp::Main`/`Networking`; server `apps/openmw-mp/Networking.cpp` | RakNet connection, TES3MP version/protocol checks, handshake/password handling, and plugin filename/checksum pre-init are implemented. Two clients passed the exact-checksum pre-init against a local server. |
| Character creation and initial load | `LocalPlayer::processCharGen`, `Main::updateWorld`, player packet processors | Char-gen stages, base info, loaded state, and initial stats have packet paths. One synthetic name-entry test reached server `OnPlayerConnect`; full multi-stage character creation and sustained play remain unverified. |
| Player state | `LocalPlayer::update` and its update/send methods | Position, cell changes, animation flags, dynamic stats, attributes, skills, level, bounty, reputation, equipment, inventory, spellbook/active spells, quick keys, journal/factions, and related state have sender/receiver code. Most routine updates are change-driven; position/other updates run on the TES3MP frame path. This is protocol coverage, not proof each operation stays consistent in play. |
| Remote players and NPC actors | `DedicatedPlayer`, `DedicatedActor`, `ActorList`, `Cell`, `CellController` | Remote replicas and per-cell authority records exist. Actor packets cover position, animation, stats, equipment, AI, attacks/casts, and cell changes. OpenMW 0.52’s central actor/mechanics update paths are not fully integrated with those authority records; see below. |
| Object and inventory interactions | `ObjectList`, `mwclass`, `mwworld`, and GUI handlers | A server-issued dagger/equipment update reached a client, and server-scripted summon spawn reached both clients. Ordinary activation, loot, player-to-player inventory transfer, and client-originated placement remain untested. |
| World state | `Worldstate`, world-state packet processors, `weather.cpp`, `globalmap.cpp` | Code exists for time/weather, cell resets, dynamic records/settings/scripts, region authority, explored map tiles, and related world state. A map-tile packet was observed; time/weather and the remaining world-state paths were not live-tested. |
| Chat and server scripting | `mwmp/GUI/GUIChat`, server Lua APIs and script callbacks | Chat/UI and a broad server scripting API are present and compile. The test plugin’s data-file and `OnPlayerConnect` callbacks ran; chat and gameplay callbacks were not tested. |

TES3MP is not a headless OpenMW game server. The server executable coordinates
clients, packets, state, and scripts; clients simulate the game. Actor control
is distributed by cell authority, so correct integration with OpenMW’s actor
simulation is essential.

## Known incomplete or unreliable gameplay

### AI and actor authority

`CellController` tracks per-cell authority and creates local versus dedicated
actor wrappers. `aicombat.cpp` also contains some local-authority NPC attack
packet hooks. However, the OpenMW 0.52 `Actors::update`/mechanics scheduler in
`apps/openmw/mwmechanics/actors.cpp` does not contain the complete TES3MP
authority gating/reconciliation needed by the old port. The port notes
explicitly list actor authority and movement as unfinished. Consequently,
Player position packets now update from 0.52 movement settings and were seen in
a live session. NPC AI movement and authority handoff between clients still
cannot be considered reliable; duplicate or missing simulation is possible
and was not tested.

### Combat

- The local-authority NPC path in `aicombat.cpp` records and sends some attack
  state.
- `weaponanimation.cpp` captures weapon/ammunition and projectile-origin data
  for ranged attacks.
- `spellcasting.cpp` has a partial network-aware cast-success path and
  selected effect/sound handling.
- OpenMW 0.52 attack state is now collected in `actors.cpp`, melee results in
  `mwclass/npc.cpp` / `mwclass/creature.cpp`, and ranged results in
  `combat.cpp`; these populate TES3MP attack packets. I attempted a live attack
  against a server-spawned rat, but the headless input run produced no
  observable attack packet. Treat combat damage, authority, block outcomes, and
  consistency between clients as **unverified and not reliable yet**.
- Spell casting is explicitly partial in `PORTING-NOTES.md`; the presence of
  some hooks does not establish synchronization of all casts, projectiles,
  impacts, resistances, or effects.

### Death and resurrection

`CharacterController::kill()` now sends the chosen death state for the local
player and, when this client has cell authority, for a local NPC. This restores
the outgoing notification at the first death-animation transition; remote
death packet handling and resurrection application already exist. The local
death packet receiver now applies the echoed death without sending it back a
second time. A test server setting health to zero caused the client to send
`ID_PLAYER_DEATH`; the
server callback ran and a regular resurrection packet was received. This was
not a combat-caused death, and kill attribution, loot, authority handoff, and
re-entry remain untested. The separate `ID_PLAYER_STATS_DYNAMIC` packet does
not serialize the dead flag; the test used it only to drive health to zero.

### Summoned creatures

Authority-owned spell summons now use the summon-aware object-spawn packet.
The server assigns the shared `mpNum`, broadcasts the summon and acknowledges
that identity to its creator. A server-scripted summon packet was reconstructed
on both clients, including the master/effect metadata. However, the test did not
trigger OpenMW's local spell-cast path, so the new client-originated summon
hook and acknowledgement path remain unverified. Non-authority ownership,
effect expiry, cell changes, and summon death also need validation.

### Other limits

- The TES3MP browser is not verified: enabling `BUILD_BROWSER` fails CMake
  configuration at `apps/browser/CMakeLists.txt` because `QT5_ADD_RESOURCES`
  is undefined. The top-level Qt gate omits `BUILD_BROWSER`, while the browser
  target still uses Qt 5 CMake macros; the option was left disabled.
- The construction-set tests and browser UI were not built.
- Player position updates were observed while moving with W, but crossing a
  cell boundary by walking was not tested. A synthetic melee attempt with a
  server-granted dagger and spawned rat did not produce an observable attack
  packet, so combat remains unverified. Player-to-player inventory exchange
  was not exercised. Death/resurrection and server-scripted summon reception
  were tested as described above, but not through combat or spell casting.
- Earlier teardown traces exposed invalid raw `ESM::Path` serialization and
  shutdown ordering issues. These were fixed; a later two-client normal
  disconnect and server-shutdown run completed with status 0 and no crash.

## Recommended next validation/port work

1. Wire OpenMW 0.52’s actor update, movement, and AI paths to TES3MP cell
   authority; test authority handoff across cell changes.
2. Verify the new player melee/ranged packet hooks and finish NPC combat
   authority/result reconciliation through the 0.52 mechanics flow.
3. Validate combat-caused player/NPC death, kill attribution, loot,
   resurrection, and re-entry across clients.
4. Cast a summon spell from the authority client and verify the outgoing
   summon packet, creator acknowledgement, ownership, expiry, cell changes,
   and death.
5. Finish spell/cast/projectile authority reconciliation.
6. Run a two-client integration matrix with matching and mismatching content,
   then movement, cell changes, inventory, objects, weather/time, combat,
   death, resurrection, and summons.

## Changes made during testing

The following issues blocked live testing and were fixed in this worktree:

- `apps/openmw/main.cpp` now registers TES3MP's `--connect`/`--password`
  options; before that, client startup threw `boost::bad_any_cast`.
- `apps/openmw/mwmp/Networking.cpp` now uses OpenMW's extension helper for
  content-file lookup. Passing a leading-dot extension made valid ESMs fail
  pre-init with “Plugin doesn't exist”.
- `components/openmw-mp/Packets/BasePacket.hpp` now serializes `ESM::RefId`
  values through their text representation rather than RakNet's raw in-memory
  bytes. This fixed cell-region corruption and the crash when a client created
  a remote player from base-info. It also covers other packet fields carrying
  typed record IDs.
- `components/openmw-mp/Packets/BasePacket.hpp` also serializes `ESM::Path`
  through its original text. Remote NPC base info includes a model path;
  sending the C++ path object bytes corrupted its string and caused teardown
  crashes.
- `apps/openmw/mwmp/LocalPlayer.cpp` now detects position changes directly and
  refreshes packet direction from OpenMW's movement settings. The old code
  relied on a direction field the 0.52 input path did not update, so normal
  walking was not sending position updates.
- `apps/openmw/mwmechanics/actors.cpp`, `mwclass/npc.cpp`, `mwclass/creature.cpp`,
  and `mwmechanics/combat.cpp` now connect local attack input and hit results
  to TES3MP packets, including hit, damage, block, and enchantment fields. This
  compiles but combat remains unverified in a live fight.
- `apps/openmw/engine.cpp` and `apps/openmw/mwmp/PlayerList.cpp` now clean up
  remote player references and actor wrappers before destroying the world and
  window manager; TES3MP logging remains alive through shutdown. A two-client
  connect/disconnect test completed cleanly.
- `LocalPlayer::die()` no longer echoes the server's death notification back
  as a second death packet, and multiplayer death no longer triggers the
  single-player recent-save reload path.
- The same packet helper now serializes `ESM::Path` as text. Remote NPC base
  info includes an NPC model path; serializing the C++ object bytes corrupted
  its string state and caused teardown `free()` crashes.
- `apps/openmw/mwmp/LocalPlayer.cpp` now detects position changes directly and
  refreshes packet direction from OpenMW's movement settings. The old code
  relied on a direction field that the 0.52 input path never updated, so normal
  movement did not send position updates.
- `apps/openmw/mwmechanics/actors.cpp`, `mwclass/npc.cpp`, `mwclass/creature.cpp`,
  and `mwmechanics/combat.cpp` now connect local player attack input and local
  hit results to TES3MP attack packets, including block and hit data. This has
  compiled but combat is still not live-verified.
- `apps/openmw/engine.cpp` and `apps/openmw/mwmp/PlayerList.cpp` now remove
  remote-player and actor references before destroying the world/window
  manager, and keep TES3MP logging alive through its shutdown. A two-client
  normal-exit test now completes without the earlier teardown crash.
- `LocalPlayer::die()` no longer echoes a death packet when applying the
  server's death notification, and the vanilla recent-save reload is skipped
  for multiplayer deaths so the client can await server resurrection.
- `apps/openmw/mwmechanics/character.cpp` now sends player death state and
  locally-authoritative NPC death state when the death animation begins. The
  hook is in `kill()` so animation restoration does not resend the event.
- `apps/openmw/mwmechanics/summoning.cpp` now sends authority-owned spell
  summons through the existing summon-aware object packet. The server relays
  the assigned multiplayer ID to other clients and acknowledges it to the
  creating client; the latter updates its local summon rather than creating a
  duplicate.
- The chat, dialog-list, login, and text-input layouts plus the chat skin were
  restored and added to `files/data/CMakeLists.txt`; the chat skin is registered
  and uses the available default font. This fixed client startup failing while
  creating the chat window.
- `apps/openmw-mp/Networking.cpp` now returns after handling a duplicate
  `ID_PLAYER_BASEINFO`, instead of logging it as an unhandled packet.
- `apps/openmw_tests/mwscript/testutils.hpp` implements the four context-
  tracking methods required by OpenMW 0.52 so the unit-test target builds.

These are local, uncommitted changes. The known mechanics gaps above remain;
the fixes unblock startup/content handshake but do not complete the gameplay
port.

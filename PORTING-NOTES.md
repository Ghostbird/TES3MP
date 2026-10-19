# TES3MP rebase onto OpenMW master — porting notes

## Where things are

- Worktree: `/tes3mp/.worktrees/merge-wt` (branch `rebase-0.52-merge`)
- Build dir: `/tes3mp/.worktrees/merge-wt/build`
- Merge artifacts (3-way merge results, file lists, scripts):
  `/tes3mp/.worktrees/rebase-artifacts`
- CrabNet source + prebuilt RakNet: `/tes3mp/.worktrees/CrabNet`,
  `/tes3mp/.worktrees/raknet-installed` (also installed to /usr/local)
- Both `build/tes3mp-server` and `build/tes3mp` build and link.
- `/root/CrabNet` and `/usr/local/{include/raknet,lib/libRakNetLibStatic.a}`
  hold the RakNet build (CrabNet is TES3MP's RakNet fork).

## Approach

The old WIP squash (`wip-squash-backup` tag, commit 33cb24ff33) applied the
TES3MP 0.8.1 delta onto OpenMW master with 129 files left containing raw
conflict markers and several silent reverts of upstream code. It was undone
and re-done as a proper 3-way merge:

- base: `7be09078b4` (OpenMW 0.47-RC3, the fork point of TES3MP 0.8.1)
- ours: `e501e667bb` (latest OpenMW master in `../openmw`, 2026-10-14)
- theirs: `0.8.1` (TES3MP release)

On top of that, the maintainer's in-progress port (local branch `0.9`,
commits ac9b29ca57 "Build using OpenMW 0.52" + 90f3cc4de2, with its own
HANDOFF.md) was used as a reference: its 91 adapted files (esm3 include
moves, API fixes, build wiring) were layered on as patches, and its
handoff document lists the 0.47 -> 0.52 API migrations.

Files deleted upstream were dropped (the `add_component_dir` CMake macro
globs sources, so inlined-header stub .cpp files are unnecessary); files
moved upstream were re-targeted (`components/esm/*` -> `components/esm3/*`,
`files/mygui/*` -> `files/data/mygui/*`).

## Build

```bash
cd /tes3mp/.worktrees/merge-wt/build
cmake .. -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_C_COMPILER=gcc-13 \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_OPENMW=ON -DBUILD_OPENMW_MP=ON \
  -DBUILD_LAUNCHER=OFF -DBUILD_WIZARD=OFF -DBUILD_OPENCS=OFF \
  -DBUILD_BROWSER=OFF -DBUILD_MASTER=OFF -DBUILD_MWINIIMPORTER=OFF \
  -DBUILD_ESSIMPORTER=OFF -DBUILD_BSATOOL=OFF -DBUILD_ESMTOOL=OFF \
  -DBUILD_NIFTEST=OFF -DBUILD_NAVMESHTOOL=OFF -DBUILD_BULLETOBJECTTOOL=OFF \
  -DBUILD_DOCS=OFF -DBUILD_COMPONENTS_TESTS=OFF -DBUILD_BENCHMARKS=OFF \
  -DBUILD_OPENCS_TESTS=OFF -DBUILD_OPENMW_TESTS=OFF \
  -DOPENMW_USE_SYSTEM_YAML_CPP=OFF -DOPENMW_USE_SYSTEM_MYGUI=OFF \
  -DRakNet_LIBRARY_DEBUG=/usr/local/lib/libRakNetLibStatic.a
make -j4 tes3mp-server
make -j4 tes3mp
```

g++-13 is required (OpenMW 0.52 uses `<format>`); it was installed from
ppa:ubuntu-toolchain-r/test.

## Done

- Build system: TES3MP options/apps, RakNet (FindRakNet at root scope so the
  client target gets includes), LuaBridge/PicoSHA2/Base64, breakpad gitlink.
- Components: full networking library, TES3MP settings (General/Plugins/
  MasterServer + a new Chat category), interpreter context tracking,
  ESM::Cell::getShortDescription alias, ESM3 CellRef::mMpNum, Cell/Pathgrid
  record overriding (Store::override implementations).
- apps/openmw-mp (server): builds, links and produces `tes3mp-server`.
  Ported: program option types (EscapePath -> MaybeQuotedPath), includes.
- apps/openmw (client): ported the TES3MP entry points and `mwmp` sources to
  OpenMW 0.52 APIs, including RefId-based records/cells, current movement and
  inventory APIs, GUI ownership, settings, and the MWClass physics hooks.
- CMake now explicitly registers all client `mwmp` sources; before this fix,
  the client linked with unresolved TES3MP symbols.

## Remaining functional port work

- A successful client link is not a full gameplay validation. The modern
  `actors.cpp`, `character.cpp`, `combat.cpp`, and `summoning.cpp` still need
  their TES3MP authority, movement, death, and summon synchronization hooks
  reconciled with OpenMW's reworked mechanics APIs. `spellcasting.cpp` has only
  a partial hook port.
- Exercise login, character creation, movement, combat, death/resurrection,
  cell changes, and summons against a server with game data. `ctest` currently
  reports no configured tests in this build.

## Validation

- `make -j4 tes3mp-server` — passes and links `tes3mp-server`.
- `make -j4 tes3mp` — passes and links `tes3mp`.
- The build emits existing third-party RakNet deprecation/shadow warnings.
- Runtime multiplayer testing still requires a TES3MP server and game data;
  it was not performed as part of this rebase.

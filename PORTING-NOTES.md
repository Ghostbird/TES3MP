# TES3MP rebase onto OpenMW master — porting notes

## Where things are

- Worktree: `/tes3mp/.worktrees/merge-wt` (branch `rebase-0.52-merge`)
- Build dir: `/tes3mp/.worktrees/merge-wt/build`
- Merge artifacts (3-way merge results, file lists, scripts):
  `/tes3mp/.worktrees/rebase-artifacts`
- CrabNet source + prebuilt RakNet: `/tes3mp/.worktrees/CrabNet`,
  `/tes3mp/.worktrees/raknet-installed` (also installed to /usr/local)
- The server builds and links: `build/tes3mp-server`
- `/root/CrabNet` and `/usr/local/{include/raknet,lib/libRakNetLibStatic.a}`
  hold the RakNet build (CrabNet is TES3MP's RakNet fork).

## Approach

The old WIP squash (`wip-squash-backup` tag, commit 33cb24ff33) applied the
TES3MP 0.8.1 delta onto OpenMW master with 129 files left containing raw
conflict markers and several silent reverts of upstream code. It was undone
and re-done as a proper 3-way merge:

- base: `7be09078b4` (OpenMW 0.47-RC3, the fork point of TES3MP 0.8.1)
- ours: `e501e667bb` (latest OpenMW master, 2026-10-11)
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
  -DOPENMW_USE_SYSTEM_YAML_CPP=OFF -DOPENMW_USE_SYSTEM_MYGUI=OFF
make -j4 tes3mp-server   # works
make -j4 tes3mp          # client: work remaining, see below
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
- apps/openmw (client): header rebuild (world, windowmanager, messagebox,
  store, esmstore, npcstats, spells, globals, class, worldimp), mwmp source
  registration in CMake, chat settings, MWClass collision hooks moved to
  insertObjectPhysics.

## Remaining client work (`make -j4 tes3mp`)

Biggest offenders (error counts include cascades):

- `mwmechanics/actors.cpp`, `mwmechanics/character.hpp/cpp`,
  `mwphysics/mtphysics.hpp`, `mwphysics/actor.cpp` — Character/Actors
  reworks (the 0.47 -> 0.52 split of Character stats/actors).
- `mwworld/weather.cpp` — weather sync must move from int weather ids to
  `ESM::RefId`/`MWWorld::Weather*` (the new WeatherManager API).
- `mwscript/*extensions.cpp` — script extensions using removed APIs.
- `mwgui/dialogue.cpp`, `containeritemmodel.cpp`, `mwclass/npc.cpp`,
  `mwworld/worldimp.cpp`, `scene.cpp`, `mwrender/weaponanimation.cpp`.
- `mwworld/cellstore.cpp` + `apps/openmw/mwworld/cells.cpp` (resurrected
  from 0.8.1 — needs porting to the current CellStore design).
- `engine.cpp` — mangled by the WIP merge; rebuild from upstream and re-apply
  the mwmp::Main lifecycle hooks (init/postInit/frame/destroy, packet script
  validation, GUIController cleanup).
- `mwclass/creature.cpp` — mangled; rebuild from upstream + tes3mp hooks
  (collision, creature stats).

Established fix patterns:

1. Structural damage from the old WIP merge (duplicated/mangled
   declarations): rebuild the file from `git show e501e667bb:<path>` and
   re-apply the genuine "Start of tes3mp" additions (see the 0.8.1-vs-2021
   diff `git diff 7be09078b4 0.8.1 -- <path>` for the authoritative list).
2. API migrations (see the maintainer's HANDOFF.md on branch 0.9):
   `Misc::StringUtils` -> `components/misc/strings/*` (compat shim exists),
   raw string ids -> `ESM::RefId`, `Settings::Manager::getX` still works,
   `ESM::Cell::getShortDescription` aliased, `ENAMstruct` ->
   `IndexedENAMstruct::mData`, `Class::CLDTstruct::mSkills` ->
   `mMinorSkills`/`mMajorSkills`, `Potion::ALDTstruct::mAutoCalc` -> `mFlags`,
   MWWorld::Ptr API changes, MyGUI widget override signatures.
3. `MWBase::World::createRecord` was removed — record cloning goes through
   `ESMStore::overrideRecord` (already implemented for all types).

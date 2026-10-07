# EchoLine

An offline multiplayer, realistic sci-fi arena shooter prototype built in **Unreal Engine 5 with C++**.

Two to four operators fight over a damaged communications facility. Firearms behave like real firearms
(damage, recoil, range, stability, spread), and the signature **Echo Link** lets a player place two
anchors and fire through them to create new angles.

> This repository contains **only source code and configuration**. There are no binary `.uasset` files.
> All geometry, lights, spawn points and weapons are generated or defined in C++, so the project opens
> and runs without importing any art.

## Requirements

- **Unreal Engine 5.4** (5.3–5.6 should also work; adjust `EngineAssociation` in `EchoLine.uproject`)
- **Windows** with **Visual Studio 2022** and the *Desktop development with C++* + *Game development with C++* workloads
  (or Rider / VS Code with the UE toolchain)
- For Android: **Android Studio** with SDK/NDK/Java, plus *Android* platform support installed in the engine

## First build (PC)

1. Install Unreal Engine 5.4 and Visual Studio 2022 (with C++ game development).
2. Right-click `EchoLine.uproject` → **Generate Visual Studio project files**.
3. Open `EchoLine.sln`, set configuration to **Development Editor**, and build.
4. Launch the editor from the built target (or double-click `EchoLine.uproject`).
5. **Create an empty level** and save it as `/Game/Maps/RelayYard`.
   - `File → New Level → Empty Level`, then `Save Current Level As… → Content/Maps/RelayYard`.
   - The game mode builds the whole map procedurally at BeginPlay, so the level itself stays empty.
   - Set it as the default (`Project Settings → Maps & Modes → Default Maps`), or overwrite
     `GameDefaultMap`/`EditorStartupMap` in `Config/DefaultEngine.ini`.
6. Press **Play**.

You should spawn in the grey Relay Yard with a crosshair, HUD, on-screen touch buttons and a weapon.

## Testing multiplayer on one PC

`Editor Preferences → Level Editor → Play`:

- **Play Net Mode**: `Play As Listen Server`
- **Number of Players**: `2`

Click **Play**, then in each window press the **DEPLOY** button. A second window acts as the client.

## Testing on phones over hotspot / LAN (the real target)

The host phone (or PC) runs a listen server; other phones connect to its IP.

1. Turn on the **hotspot** on the host phone. Other phones join its Wi-Fi.
2. Package/launch the game on the host. It is a listen server once you press **HOST**.
3. Find the host's hotspot IP (Android usually uses `192.168.43.1`).
4. On each client open **JOIN BY IP**, type the host IP (e.g. `192.168.43.1`) and press **GO**.
5. Everyone presses **DEPLOY**.

Console equivalents (useful for quick testing):

```
EchoHost
EchoJoin 192.168.43.1
```

### Bluetooth — important limitation

Unreal Engine's replication uses **UDP/IP**. It does **not** ship a Bluetooth transport. Bluetooth is
therefore **not supported by this prototype**. Two options for a later phase:

- Have the host phone run a **Bluetooth PAN / tethering** network so both devices still have IP addresses
  (then it behaves like the hotspot case), or
- Write a **custom NetDriver / transport plugin** for Bluetooth RFCOMM/L2CAP. This is advanced work.

LAN/hotspot first is the sensible path, and it is what this build targets.

## Android packaging (outline)

1. Install Android Studio + SDK/NDK and configure
   `Project Settings → Platforms → Android SDK` paths.
2. `Platforms → Android → Package Project` (choose a keystore, ASTC texture format).
3. Install the APK on each phone, then follow the hotspot steps above.
4. For performance, keep the in-game **SETTINGS → Graphics** on `LOW`/`MEDIUM` on weaker phones.

## AI bots and pickups

The prototype can run without any human opponent. On startup the host spawns **3 AI bots**
(`BotCount` in `AEchoGameMode`), and the main menu has an **ADD BOT (HOST)** button to add more.

Each bot is a small state machine (`AEchoBotController`) that:

- **Attacks** a visible enemy inside range (turns to face it, waits a short reaction delay, then fires).
- **Seeks a medkit** when its health drops below 50% (unless an enemy is already in range).
- **Seeks an ammo box** when its magazine is nearly empty, and **reloads** when it runs dry.
- **Chases** a recently seen enemy to its last known position.
- **Patrols** to random reachable points when it has nothing else to do.

Navigation is **asset-free**: `FEchoNavGrid` rasterises the level's collision boxes into a
walkable grid and finds routes with **A\*** (8-directional, with a "string-pulling" smooth pass).
No baked NavMesh or editor-authored navigation volume is required.

Pickups live in `AEchoPickup`: green **medkits** (restore health) and amber **ammo boxes**
(refill the magazine). They are server-authoritative, reappear after 15 seconds, and hide
while on cooldown. The level builder places four of them.

Because bots are spawned by the server and replicate like any pawn, **clients see them move**.
Bots appear on the HUD scoreboard (they have no PlayerState, so their scores are tracked in the
replicated game state).

**Why these are source files, not `.uasset`:** Unreal's `.uasset` format is binary and is only
produced by the Unreal Editor. This repository is therefore kept asset-free and generated from
code; the PNG/WAV sources are imported in the editor when you want to move beyond the greybox look.

### Sound plays without importing

`UEchoAudioSubsystem` loads the WAVs from `ArtSource/Audio/` **at runtime**, decodes them, and
plays them through procedural sound waves. So the game has audio immediately, with no editor
import step:

- per-weapon gunshots (spatialised for other players, immediate for your own shots),
- reload, hitmarker, elimination, Echo anchor place/destroy, pickup, UI click,
- local footsteps while moving on the ground.

If the folder is missing, the subsystem logs a warning and the game simply plays silently.
Run `python3 Tooling/generate_assets.py` to create the files.

For a **packaged** build the folder is staged as a loose file directory
(`DirectoriesToAlwaysStageAsNonUFS` in `Config/DefaultGame.ini`), so the loader finds it on device.

## Source assets (textures and audio)

`Tooling/generate_assets.py` generates importable **source assets** using only the Python
standard library:

```
python3 Tooling/generate_assets.py
```

It writes `ArtSource/Textures/*.png` (operator base colours, gunmetal, Echo panel emissive,
anchor core, medkit/ammo icons) and `ArtSource/Audio/*.wav` (per-weapon gunshots, reload,
hitmarker, kill, Echo place/destroy, pickup, UI click, footstep). See `ArtSource/README.md`
for the mapping and Unreal import steps.

**Why these are source files, not `.uasset`:** Unreal's `.uasset` format is binary and is only
produced by the Unreal Editor. This repository is therefore kept asset-free and generated from
code; the PNG/WAV sources are imported in the editor when you want to move beyond the greybox look.

## Visuals in the build (no imports needed)

Even without importing anything, operators and weapons are built from primitives in C++
(`EchoArt.h`, `EchoLineCharacter.cpp`):

- Each operator has a torso, pelvis, limbs, head, head gear and backpack, in the palette from the
  design document, plus role-specific gear (**Rook** plain, **Wisp** antenna, **Patch** orange
  harness, **Cairn** overcoat).
- Three distinct first-person weapon models: a long **Meridian** rifle, a short **Kestrel** SMG,
  and a scoped **Lancer** marksman rifle.

## Controls

**Touch**

- Left half of the screen: dynamic movement stick (base appears where you touch)
- Right half: drag to look
- Buttons: `FIRE`, `ADS`, `RELOAD`, `ECHO`, `CROUCH`, `JUMP`, `SWAP`, and the top-right menu button

**Keyboard / mouse (desktop testing)**

- `W A S D` move, mouse look, `LMB` fire, `RMB` aim
- `R` reload, `E` arm Echo, `Space` jump, `Ctrl` crouch, `Q` cycle weapon
- In menus, click with `LMB`

## How Echo Link works

1. Press **ECHO** to arm it (the HUD shows `ECHO ARMED`).
2. Fire at a **blue Echo panel** (thin panels near the dish and on the flanks). This drops anchor A
   and disarms Echo.
3. Press **ECHO** again and fire at another panel to drop anchor B. The pair is now linked (anchors
   turn green).
4. Normal shots fired into one anchor **exit from the other**, keeping direction — you can shoot
   around cover. The link lasts 12 seconds.
5. Enemies can **shoot your anchors** to destroy them. You cannot destroy your own.

Tuning values live in `UEchoLinkComponent` (`AnchorLifetime`, `MaxAnchors`, `PlaceCooldown`,
`MaxPlaceDistance`).

## Weapons

Defined in `Source/EchoLine/EchoWeapons.h`. Each has damage, RPM, magazine, reload, recoil,
stability, effective/max range with falloff, hip/ADS spread, and a movement spread multiplier.

| Weapon | Role | Damage | RPM | Mag | Effective range |
|---|---|---:|---:|---:|---:|
| AR-7 Meridian | Rifle | 30 | 600 | 30 | 35 m |
| Kestrel-9 | SMG | 23 | 800 | 32 | 18 m |
| MR-12 Lancer | Marksman | 55 | 180 | 12 | 60 m |

## Settings

`SETTINGS` (from the main menu or the in-game menu button) adjusts touch/camera **Sensitivity**,
**Field of View**, **Invert Y**, and **Graphics** quality (resolution scale + scalability). Values are
saved to the engine user settings ini.

## Code map

| File | Responsibility |
|---|---|
| `EchoTypes.h` | `FWeaponStats`, `EWeaponId`, `EFireMode`, `EOperatorId` |
| `EchoWeapons.h` | Hard-coded weapon table + falloff math |
| `AEchoLineCharacter` | Operator pawn: first-person camera, health, movement, inputs |
| `UEchoWeaponComponent` | Fire/reload/recoil/spread, server-authoritative shots and damage |
| `UEchoLinkComponent` | Echo Link: place, link, redirect, destroy anchors |
| `AEchoAnchor` | A placed, replicating anchor (glow, pulse, lifetime) |
| `AEchoTracer` | Cosmetic bullet tracer |
| `AEchoLinePlayerController` | Touch controls, menus, settings, host/join |
| `AEchoHUD` | Canvas HUD: crosshair, health, ammo, score, timer, buttons |
| `AEchoGameMode` | Free-for-all flow, scoring, respawns, round timer |
| `AEchoGameState` | Replicated timer/scores/kill feed; builds the level on every machine |
| `ARelayYardBuilder` | Procedural greybox of Relay Yard (geometry, lights, spawns, panels, pickups) |
| `AEchoBotController` | AI: state machine (attack/heal/ammo/chase/patrol) driven by the nav grid |
| `FEchoNavGrid` | Grid rasterisation + A\* pathfinding (asset-free navigation) |
| `AEchoPickup` | Replicated medkit / ammo box with timed respawn |
| `EchoArt` | Procedural operator/weapon visuals from primitives |
| `UEchoAudioSubsystem` | Loads generated WAVs at runtime and plays them (procedural sound waves) |
| `Tooling/generate_assets.py` | Generates PNG textures and WAV sound effects (stdlib only) |

## Known prototype limitations

- **Movement is client-authoritative.** Positions are simulated locally and shared; the server
  resolves combat. A production build should move movement to the server with reconciliation.
- **No lag compensation** yet; hit resolution uses the shooter's reported origin/direction.
- The **Relay Yard** greybox is single-elevation. The designed upper maintenance platform is deferred.
- **Bots** path on a 2D grid: they do not jump, use crouch, or use Echo Link, and their aim is
  simple (no leading or prediction). They are meant for offline testing, not as skilled opponents.
- Anchors use a simple sphere test and portal-style direction (exit preserves the incoming direction).
  This may change after playtesting.
- Placeholder capsule/cube meshes stand in for operators and weapons; there are no animations yet.
- **Bluetooth is not implemented** (see above).

## Where this sits in the plan

This is the **first-playable + Echo Link prototype** milestone from the design plan:
two machines can connect over LAN/hotspot, move in a 3D arena, shoot with three weapons that have
distinct stats, use the Echo Link mechanic, and finish a timed match with a results screen.

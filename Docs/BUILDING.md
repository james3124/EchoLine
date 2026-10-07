# Building and playing EchoLine

This project is **Unreal Engine 5 C++**. Unreal has **no Android editor and no phone build
path**, so the game must be compiled on a PC-class machine once, producing an APK you install on
your phone. After that you can play on the phone forever.

Below are the realistic options, from easiest to hardest.

---

## Getting the Unreal Engine files

If you want to try GitHub or Docker, this is where the engine comes from:

- **Epic Games Launcher** — the normal install (editor + build tools). **Desktop only**; cannot be
  installed or run on a phone.
- **GitHub source:** `https://github.com/EpicGames/UnrealEngine` — a **private** repository. You must
  link your Epic Games account to GitHub (Epic: *Apps & Accounts → Accounts → Connect GitHub*, then
  accept the emailed invite to the @EpicGames org). Then you can clone the source, but you must
  **compile the engine yourself** (Visual Studio, hours of build time, tens of GB).
- **Container images:** `ghcr.io/epicgames/unreal-engine:dev-5.4.x` (the prebuilt Linux engine used by
  CI). Also gated: link the Epic account, create a GitHub PAT with `read:packages`, then
  `docker login ghcr.io`. Pulls are several GB.

**Important limitation:** the official Linux development containers target **Linux and Windows**
builds. They do **not** ship the Android SDK/NDK, and building an **Android APK** inside them is not
supported without installing the full Android toolchain yourself. Free GitHub runners also only
guarantee ~14 GB free disk. For those two reasons, an Android build on free GitHub cloud is very
unlikely to succeed.

None of these routes produce a result that runs on a phone — the engine (or an APK built from it)
still requires a PC-class machine to build in the first place. See Options 1–4 above.

## Option 1 — Build once on any PC (recommended)

The single easiest route. You need **temporary access to any Windows PC or laptop** (a friend's, a
school/library PC, a borrowed machine). It does not need to be powerful; the build just takes a
while.

1. Install **Unreal Engine 5.4** (Epic Games Launcher) and **Visual Studio 2022** with the
   *Game development with C++* workload.
2. Install **Android Studio** and, inside UE, `Edit → Project Settings → Platforms → Android`,
   run the **Install SDK** button (it fetches the right SDK/NDK automatically).
3. Clone this repository and open `EchoLine.uproject`.
4. In the editor, create an empty level and save it as `Content/Maps/RelayYard`
   (the game mode builds the whole map at runtime; the level itself stays empty).
5. `Platforms → Android → Package Project → Android (ASTC)`.
6. Copy the resulting `.apk` to your phone and install it (enable "install unknown apps").

See the main `README.md` for the full editor steps and how to play over hotspot/LAN.

---

## Option 2 — Cloud PC you control from your phone (no owned PC)

You can rent a Windows machine by the hour and drive it from your phone:

- **Azure / AWS / Google Cloud** free trials, or hourly GPU/Windows VMs.
- Connect from your phone with **Microsoft Remote Desktop** or **Chrome Remote Desktop**.
- On that cloud PC, follow **Option 1**, then move the APK to your phone (cloud storage or a
  direct download link).

Caveats: large downloads (Unreal + Android SDK are tens of GB), trial time/credit limits, and the
remote desktop experience on a phone is fiddly. It works, but it is not pleasant.

---

## Option 3 — GitHub Actions

**GitHub does not remove the need for a PC here.** The included workflow
(`.github/workflows/build-android.yml`) is correct and will produce an APK, but only on a runner
that already has Unreal installed:

- **Free GitHub-hosted runners cannot build Unreal.** They are guaranteed only ~14 GB free disk
  (2-core, private repos) and Epic's licence/size rules out the engine there.
- A **self-hosted runner** means registering *your own* PC with GitHub — which is Option 1 with
  extra steps.
- A **paid larger runner** (150 GB+) can work, but you still have to install the engine and
  Android SDK on it as part of setup.

So GitHub is useful for *automating repeatable builds* once you have a machine — not as a
replacement for one.

---

## Option 4 — Change engine so the phone really is enough

If you have **only a phone and no access to any PC ever**, Unreal is the wrong tool. Two paths
actually reach "play on my phone":

### Godot (editable and exportable on Android)
Godot 4 has an official **Android editor** on the Play Store. It can edit projects **and export
APKs on-device** (a separate "Godot Export" helper app is needed for Gradle exports). This game
would need to be **ported** from Unreal C++ to GDScript/C#, but it would let you build entirely
on your phone. Best when using a Bluetooth keyboard/mouse.

### Web build (play instantly in the browser)
Rewrite the game as a **Three.js/WebGL** page (the very first attempt at this project was headed
that way). No build, no install: you open a link in your phone's browser and play. The catch is
that hotspot/LAN multiplayer and raw 3D performance are more limited than native, and it is also
a rewrite.

Both are real rewrites, not conversions. Ask if you want to go down either path.

---

## Bluetooth reminder

Unreal replicates over UDP/IP and ships **no Bluetooth transport**. Even after building, the
multiplayer is **hotspot / Wi-Fi LAN** (host phone + joiners). Bluetooth would need a custom
NetDriver plugin or a Bluetooth PAN that still provides IP addresses. This is a design limitation,
not a build one.

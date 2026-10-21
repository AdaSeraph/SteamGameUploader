# Steam Game Uploader

**English** | [简体中文](README.zh-CN.md)

A small native Windows tool for uploading game builds to Steam through SteamPipe (SteamCMD). It keeps one profile per game, checks the build folder before anything is sent, and only unlocks the real upload after a successful SteamPipe Preview of the exact same settings.

- Native Win32 / C++20: a single ~500 KB `.exe`. You don't need to install .NET, Qt, or the VC++ runtime.
- Never asks for or stores your Steam password or Steam Guard code.
- UI in English, 简体中文 and 日本語.

## Features

- **Multiple game profiles** with App ID, depot IDs, Steamworks account, content root, SteamCMD path, build description, exclusion rules, and SetLive branch. You can import and export a profile as a `.suprofile` file.
- **Build check** counts files and total size and finds the main `.exe`. It blocks the upload if `steam_appid.txt` would be shipped, and warns about leftover `Saved` content or a filled-in SetLive branch.
- **Preview first:** **Upload** stays disabled until a SteamPipe Preview passes. Changing any upload setting afterwards locks it again.
- **Confirmation before upload:** a summary dialog appears before every upload. SetLive is empty by default, so nothing goes live by accident.
- **Live SteamCMD log**, per-profile upload history, and a **Cancel** button that stops SteamCMD and all of its child processes.

## Download

Get the latest `SteamGameUploader-<version>-win-x64.zip` from [Releases](https://github.com/AdaSeraph/SteamGameUploader/releases), unzip it, and run `SteamUploader.exe`. Each zip has a `.sha256` file next to it so you can verify the download.

Requirements:

- Windows 10 or 11, x64.
- `steamcmd.exe`. It ships with the [Steamworks SDK](https://partner.steamgames.com/doc/sdk) at `sdk/tools/ContentBuilder/builder/steamcmd.exe`, or you can download [SteamCMD](https://developer.valvesoftware.com/wiki/SteamCMD) on its own.
- A Steamworks account that is allowed to upload builds for your app.

> The executable is not code-signed, so Windows SmartScreen may warn on first launch. Choose **More info → Run anyway**, or build it yourself from source.

## Quick start

1. Launch `SteamUploader.exe`. On first run it creates an empty **New Game** profile.
2. Fill in **App ID**, **Depot IDs** (comma-separated), **Steam Account**, **Content Root** (the folder that contains your packaged game), and the path to **SteamCMD**.
3. Add **Exclusions**, one pattern per line. Always keep `steam_appid.txt` in the list.
4. Click **Save Profile**.
5. Click **Log In**. A separate SteamCMD console opens; type your password and Steam Guard code there. SteamCMD caches the login, which Preview and Upload need because they run SteamCMD in the background without any input. Repeat this step whenever the cached login expires.
6. Click **Check Build** and fix any errors it reports.
7. Click **Preview**. SteamPipe builds the manifests without uploading. If it succeeds, **Upload** is unlocked.
8. Click **Upload** and confirm.

Uploading does not release the build. Unless you filled in SetLive, open **Steamworks Builds** and set the new build live on a branch yourself.

## Exclusion rules

- One pattern per line. Empty lines and lines starting with `#` are ignored.
- `*` and `?` wildcards are matched against the path relative to the content root and against the file name, so `*.pdb` excludes PDB files in every folder.
- `Folder/*` excludes everything inside that folder.
- Every pattern is passed to SteamPipe as a `FileExclusion` for each depot. Prefer paths relative to the content root (for example `MyGame/Saved/*`), because both the local check and SteamPipe interpret those the same way.

A typical Unreal Engine profile:

```text
*.pdb
Manifest_*.txt
steam_appid.txt
MyGame/Saved/*
```

## Multiple depots

**Depot IDs** accepts a comma-separated list such as `1234561, 1234562`. Every listed depot is mapped to the same content root with the same exclusions. If your depots need different folders or rules, create a separate profile for each.

## Data and security

Everything is stored in `%LOCALAPPDATA%\SteamUploader` (the **Data Folder** button opens it):

| Path | Contents |
| --- | --- |
| `profiles.ini` | Game profiles. No passwords or Steam Guard codes. |
| `settings.ini` | App preferences, such as the UI language. |
| `history\` | Preview and upload results for each profile. |
| `build-cache\` | SteamPipe incremental build cache. Keep it to speed up later uploads. |
| `runs\` | Temporary VDF scripts, deleted when each task ends. |

Safety rules built into the tool:

- Credentials are only typed into SteamCMD's own window. The tool never reads or saves them.
- An upload that includes `steam_appid.txt` fails the build check.
- An upload must be preceded by a Preview of the same settings, and you must confirm it.
- SetLive is empty by default, so no branch goes live unless you set one.
- Closing the window during a task asks for confirmation and cancels SteamCMD cleanly.

## Language

The UI language follows your Windows display language (English is the fallback). You can switch it at any time from the picker in the top-right corner, and the choice is saved in `settings.ini`.

To add a translation, add an entry to `su::Lang` and a `LanguageInfo` row in [`src/I18n.h`](src/I18n.h) and [`src/I18n.cpp`](src/I18n.cpp), then add a column to every row of the string table. The build fails if a string is missing.

## Building from source

You need Visual Studio 2022 or later with the **Desktop development with C++** workload and a Windows 10/11 SDK. The project uses whichever toolset your Visual Studio provides.

```powershell
powershell -ExecutionPolicy Bypass -File .\Build-Release.ps1
```

The output is `bin\Release\SteamUploader.exe`. It links the C runtime statically (`/MT`), so it runs without extra DLLs. You can also open `SteamUploader.sln` in Visual Studio.

To create the release zip and checksum in `dist\`:

```powershell
powershell -ExecutionPolicy Bypass -File .\Build-Release.ps1 -Package -Version v1.0.0
```

## Releasing

GitHub Actions builds every push and pull request. Pushing a `v*` tag also publishes a GitHub Release with the zip and its `.sha256`. The annotated tag message becomes the release notes, and tags that contain a `-` (for example `v1.1.0-beta.1`) are marked as pre-releases.

```powershell
git tag -a v1.0.1 -m "Release notes go here"
git push origin v1.0.1
```

## Disclaimer

This project is not affiliated with or endorsed by Valve Corporation. Steam and Steamworks are trademarks of Valve Corporation. Always check a build in Steamworks before setting it live.

## License

[MIT](LICENSE)

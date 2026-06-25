# Building RoninOBSChat

## Prerequisites

| Tool | Version | Download |
|------|---------|----------|
| CMake | 3.16+ | cmake.org |
| Visual Studio | 2022 (C++ workload) | visualstudio.microsoft.com |
| Qt | 6.x (MSVC 64-bit) | qt.io/download-open-source |
| OBS Studio dev package | 30.x | github.com/obsproject/obs-studio/releases |

## 1 — Download the OBS development package

From the OBS releases page download `obs-dev-<version>-windows-x64.zip`.  
Extract it somewhere, e.g. `C:\obs-dev`.

## 2 — Configure

```powershell
cmake -B build -S . `
    -G "Visual Studio 17 2022" -A x64 `
    -DOBS_ROOT="C:/obs-dev" `
    -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
```

## 3 — Build

```powershell
cmake --build build --config RelWithDebInfo
```

## 4 — Install

```powershell
cmake --install build --config RelWithDebInfo
```

This copies the `.dll` to `%APPDATA%\obs-studio\plugins\RoninOBSChat\bin\64bit\`
and locale data to `…\data\RoninOBSChat\locale\`.

## 5 — Google Cloud setup (required for YouTube)

1. Go to https://console.cloud.google.com and create a project.
2. APIs & Services → Enable **YouTube Data API v3**.
3. OAuth consent screen → External → add scope `https://www.googleapis.com/auth/youtube`.
4. Credentials → Create OAuth client ID → **TV and Limited Input devices**.
5. Copy the **Client ID** and **Client Secret**.
6. In OBS: open the *Ronin Chat Bot* dock → **Settings** → **YouTube** tab → paste credentials → **Connect YouTube** → follow the on-screen instructions.

## Config file location

`%APPDATA%\obs-studio\plugin_config\RoninOBSChat\config.json`

The file is created automatically on first save.  Tokens are stored here; keep it private.

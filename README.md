# RoninOBSChat

A native OBS Studio plugin that connects to your YouTube live chat and provides:

- **Chat Commands** — respond to `!commands` with custom replies, per-user cooldowns, and permission levels
- **Timed Messages** — automatically post messages on a configurable interval
- **AutoMod** — keyword/regex rules that can delete messages, timeout, or ban users

---

## Requirements

- OBS Studio 30 or newer (Windows 64-bit)
- A Google account with YouTube live streaming enabled
- Google Cloud OAuth 2.0 credentials (Client ID + Client Secret)

---

## Installation

1. Go to the [Releases](../../releases) page and download the latest `RoninOBSChat-*-windows-x64.zip`

2. Extract the ZIP — it contains a `RoninOBSChat` folder with `bin` and `data` subfolders

3. Open the extracted `RoninOBSChat\bin\64bit\` folder. Copy the files into your OBS installation:

   | File in ZIP | Copy to |
   |---|---|
   | `RoninOBSChat\bin\64bit\RoninOBSChat.dll` | `C:\Program Files\obs-studio\obs-plugins\64bit\` |
   | `RoninOBSChat\bin\64bit\tls\qschannelbackend.dll` | `C:\Program Files\obs-studio\obs-plugins\64bit\tls\` (create the `tls` folder if it does not exist) |
   | `RoninOBSChat\data\` (entire folder) | `C:\Program Files\obs-studio\data\obs-plugins\RoninOBSChat\` |

   The final layout should look like:

   ```
   C:\Program Files\obs-studio\
   ├── obs-plugins\
   │   └── 64bit\
   │       ├── RoninOBSChat.dll
   │       └── tls\
   │           └── qschannelbackend.dll
   └── data\
       └── obs-plugins\
           └── RoninOBSChat\
               └── locale\
                   └── en-US.ini
   ```

   > You may need administrator rights to copy files into `C:\Program Files\`.

4. Restart OBS Studio

5. The **Ronin Chat Bot** dock will appear. If it is not visible, enable it under **Docks** in the menu bar

---

## Google OAuth Setup

You need a free Google Cloud project to get credentials:

1. Go to [Google Cloud Console](https://console.cloud.google.com/)
2. Create a new project (or select an existing one)
3. Enable the **YouTube Data API v3** — search for it under *APIs & Services → Library*
4. Go to *APIs & Services → Credentials → Create Credentials → OAuth client ID*
5. Choose **Desktop app** as the application type
6. Copy the **Client ID** and **Client Secret**

---

## First-Time Authentication

1. Open the **Ronin Chat Bot** dock in OBS and click **Settings**
2. On the **YouTube** tab, paste your Client ID and Client Secret
3. Click **Connect YouTube** — a URL and a short code will appear
4. Open the URL in a browser, enter the code, and sign in with your Google account
5. Once authorised, click **Save** and then **Connect** in the dock
6. Start your YouTube live stream — the bot will connect to the active broadcast automatically

---

## Features

### Chat Commands

In Settings → Commands, add a row with:

| Field | Description |
|---|---|
| Trigger | The chat keyword, e.g. `!discord` |
| Response | The message the bot will post |
| Cooldown | Seconds before the same command can fire again |
| Level | Who can trigger it: Everyone / Mods & Up / Broadcaster Only |

### Timed Messages

In Settings → Timers, add a row with:

| Field | Description |
|---|---|
| Message | The text to post |
| Interval | How often to post it (seconds) |

Timers start automatically when you connect and stop when you disconnect.

### AutoMod

In Settings → AutoMod, add a rule with:

| Field | Description |
|---|---|
| Pattern | A plain keyword or a regular expression |
| Regex | Enable if the pattern is a regex |
| Case sensitive | Toggle case sensitivity |
| Action | Delete message / Timeout user / Ban user |
| Timeout (s) | Duration for timeout actions |

---

## Building from Source

Requires: Git, CMake 3.28+, Visual Studio 2022 (Windows)

```powershell
git clone https://github.com/t0xicVybez/RoninOBSChat.git
cd RoninOBSChat
cmake --preset windows-x64
cmake --build --preset windows-x64
```

Dependencies (OBS source, Qt 6, obs-deps) are downloaded automatically by CMake on first configure.

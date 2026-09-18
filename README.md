# SubGHz Bruteforce (Flipper Zero)

Transmits **every `.sub` file in a folder**, one after another — a Sub-GHz
"dictionary" bruteforcer, the radio equivalent of the infrared universal-remote
bruteforcer. Point it at a folder of captured/known signals and it replays them
all in sequence.

Built for **Momentum firmware** (SDK `mntm-012`) using `ufbt`.

## Features

- Pick any folder on the SD card (browse to it, press **Right** to select the folder).
- Sequentially transmits all `.sub` files found in that folder.
- Supports both protocol-encoded captures (Princeton, CAME, NICE, KeeLoq, …) and
  `RAW` captures, including custom CC1101 presets (`Custom_preset_data`).
- Configurable **delay between files**, **repeats per file**, and **loop forever**.
- **Loading screen** that scans the folder first and shows how many signals were
  found before transmitting.
- Live progress screen: file X/Y, progress bar, current filename, frequency,
  protocol, OK/error counts.
- Controls: **Left/Right** = skip to previous/next signal (hold to fast-skip),
  **OK** = pause/resume, **Back** = stop and return.

## Files

| File | Purpose |
|------|---------|
| `application.fam` | App manifest (id, entry point, category, icon). |
| `subghz_bruteforce.c` | UI (menu / settings / run screen), folder picker, worker thread. |
| `subghz_tx.c` / `subghz_tx.h` | Sub-GHz transmit engine — parses a `.sub` file and keys the radio. |
| `icon.png` | 10×10 app icon. |

## Build online (recommended — no toolchain needed)

You can compile this straight from the GitHub repo in your browser; nothing to
install locally.

1. Make sure this repo is **public** and your latest changes are **pushed**.
2. Go to **https://flipc.org** (or the mirror **https://fzoc.kanjian.fr**).
3. Paste your repository URL, e.g. `https://github.com/<you>/subghz-bruteforce`.
4. Pick the firmware target: choose **Momentum** if it's listed. If it isn't,
   pick **Unleashed** — Momentum is a fork of Unleashed and runs Unleashed apps.
5. Build, then **download `subghz_bruteforce.fap`**.
6. Copy it to `SD/apps/Sub-GHz/` on the Flipper (or drag it in via qFlipper).

These sites just run `ufbt` on the repo for you, so the repo only needs a valid
`application.fam` at its root — which it already has.

## Build with GitHub Actions

This repo also includes `.github/workflows/build.yml`, which builds the FAP on
every push and uploads it as a run artifact (**Actions** tab → latest run →
**Artifacts → subghz_bruteforce-fap**). It targets Momentum via ufbt.

## Build locally

The source was written against the real Momentum `mntm-012` SDK headers. On a
machine with internet access to `update.flipperzero.one` (needed for the ARM
toolchain):

```powershell
# One-time: install the build tool and point it at Momentum's SDK
python -m pip install --user ufbt
python -m ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json --channel=release

# From this project folder (the one with application.fam):
python -m ufbt            # builds dist\subghz_bruteforce.fap

# With a Flipper connected over USB, build + install + launch:
python -m ufbt launch
```

The resulting `subghz_bruteforce.fap` goes in `SD Card/apps/Sub-GHz/` on the
Flipper (it appears under **Apps → Sub-GHz**).

> The SDK is already deployed locally under `~/.ufbt`; only the toolchain
> download is required to finish the first build.

## Usage

1. Put your `.sub` captures in a folder on the SD card (e.g. `SD/subghz/mydict/`).
2. Open **Apps → Sub-GHz → SubGHz Bruteforce**.
3. **Select folder** → browse to the folder → press **Right** to choose it.
4. (Optional) **Settings** → set delay / repeats / loop.
5. **Start**. A loading screen scans the folder and shows the signal count, then
   transmission begins. **Left/Right** skip between signals, **OK** pauses,
   **Back** stops.

## Legal / safety

Only transmit on frequencies and to devices you are **legally authorized** to
operate. Replaying access-control signals (gates, garages, cars, etc.) that you
do not own or have permission to test may be illegal in your jurisdiction.
Fixed-code signals are also replayable in ways rolling-code systems are not —
know what you are transmitting.

# Caffeine Timer

A lightweight [KDE Plasma 6](https://kde.org/plasma-desktop/) widget that keeps your
system awake for a set amount of time — **without any Python**.

It pairs a small QML plasmoid with a native C++/Qt6 D-Bus helper. The helper is
launched only when inhibition starts and exits automatically on **Stop** or when the
timer expires, so it consumes zero resources while idle.

## Features

- **Timed wake mode** — inhibit power management for 15m, 30m, 1h, 2h, 4h, or indefinitely (`∞`).
- **Two inhibition modes**
  - `full` — inhibit power management *and* the screen saver.
  - `sleep` — inhibit power management only (display still sleeps).
- **Native helper** — a tiny C++/Qt6 binary that talks to the KDE session D-Bus APIs directly.
- **Zero idle footprint** — the helper process only runs while a timer is active.
- **Per-user isolation** — communicates over a Unix socket in `XDG_RUNTIME_DIR`; one helper per user.
- **Optional CLI** — the same helper is scriptable from the terminal.

## Requirements

- KDE Plasma 6 (Plasma/Applet `6.0` API or newer)
- Qt6 (Core, DBus, Network)
- CMake ≥ 3.20
- A C++17 compiler (GCC or Clang)
- `kde-cli-tools` (provides `kpackagetool6`)

## Installation

### Arch / CachyOS

```bash
sudo pacman -S --needed cmake gcc qt6-base kde-cli-tools
./install.sh
```

The install script builds the helper, copies it to `~/.local/bin`, and registers the
plasmoid with `kpackagetool6`.

### Add the widget

Panel → **Add Widgets** → **Caffeine Timer**.

> If the old widget remains cached, restart `plasmashell` or re-login. Make sure
> `~/.local/bin` is on the Plasma session `PATH` (it is after a re-login).

## Usage

### GUI

1. Open the widget and pick a duration (or `∞` for unlimited).
2. Toggle **"Keep display awake too"** to switch between `full` and `sleep`.
3. Press **Start**. The badge turns green and shows the remaining time.
4. Press **Stop** to end inhibition early — or let the timer expire on its own.

### CLI

```bash
~/.local/bin/caffeine-timer-helper start 30 full
~/.local/bin/caffeine-timer-helper status
~/.local/bin/caffeine-timer-helper stop
```

- `start <minutes> <mode>` — begin inhibition; duration `0` = unlimited, mode is `full` or `sleep`.
- `status` — print a compact JSON status (`active`, `until`, `mode`).
- `stop` — release all inhibitors immediately.

## How it works

```
┌──────────────┐   executable DataSource   ┌──────────────────────────┐
│  QML plasmoid │ ───────────────────────▶ │  caffeine-timer-helper   │
│  (contents/)  │   Unix socket (XDG_RUNTIME_DIR) ──▶ │  (native/main.cpp)       │
└──────────────┘                          └───────────┬──────────────┘
                                                      │ session D-Bus
                                                      ▼
                                    org.freedesktop.PowerManagement.Inhibit
                                    org.freedesktop.ScreenSaver
```

- The QML widget invokes the helper through KDE's `executable` DataSource.
- The helper is a single long-lived process that requests the KDE power/screen
  inhibitors over the session bus, then exits when inhibition ends.
- All communication happens over a per-user Unix socket in `XDG_RUNTIME_DIR`.

## Notes & limitations

- Uses KDE session D-Bus APIs only; **no direct `systemd-logind` inhibitor** is used.
- This does **not** guarantee protection against lid-close actions or manually
  initiated suspend.
- If the session bus interfaces are unavailable, `start` returns an error.
- A session shutdown naturally closes the helper and releases any inhibitors.
- KDE service API behavior may vary by Plasma release; verify build and runtime on
  your system.

## License

MIT

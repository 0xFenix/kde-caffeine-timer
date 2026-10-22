#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
cmake -S native -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j "$(nproc)"
install -Dm755 build/caffeine-timer-helper "$HOME/.local/bin/caffeine-timer-helper"
kpackagetool6 --type Plasma/Applet --upgrade . || kpackagetool6 --type Plasma/Applet --install .
echo 'Installed. Ensure ~/.local/bin is in Plasma PATH; log out/in if needed.'

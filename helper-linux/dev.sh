#!/bin/bash
# The Linux counterpart of `make dev`: builds the helper, then runs it and the
# engine from source, with reload. Ctrl-C stops both.
#
# Then, in a new Konsole tab:
#   source <this repository>/shell/bash/figxit.bash
set -eu
root=$(cd "$(dirname "$0")/.." && pwd -P)
build=$root/helper-linux/build
PATH="$HOME/.bun/bin:$PATH"

cmake -S "$root/helper-linux" -B "$build" >/dev/null
cmake --build "$build"
pkill -x figxit-helper || true
pkill -f "src/main.ts daemon" || true

"$build/figxit-helper" &
helper=$!
trap 'kill "$helper" 2>/dev/null; exit 0' INT TERM EXIT
cd "$root/engine"
bun --watch src/main.ts daemon

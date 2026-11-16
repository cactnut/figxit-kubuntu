#!/bin/bash
# The Linux counterpart of `make dev`: builds the helper, then runs it and the
# engine from source, with reload. Ctrl-C stops both.
#
# Then, in a new Konsole tab:
#   source <this repository>/shell/bash/figxit.bash
#
# The helper writes where it puts each list to ~/.local/state/figxit/helper.log.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd -P)
build=$root/helper-linux/build
state=$HOME/.local/state/figxit
PATH="$HOME/.bun/bin:$PATH"

cmake -S "$root/helper-linux" -B "$build" >/dev/null
cmake --build "$build"
pkill -x figxit-helper || true
pkill -f "src/main.ts daemon" || true

mkdir -p "$state"
FIGXIT_DEBUG=1 "$build/figxit-helper" 2>"$state/helper.log" &
helper=$!
echo "helper log: $state/helper.log"
trap 'kill "$helper" 2>/dev/null; exit 0' INT TERM EXIT
cd "$root/engine"
bun --watch src/main.ts daemon

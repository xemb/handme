#!/usr/bin/env bash
# Build (if needed) and start the handme server.
#   PORT=9000 ./start.sh
set -euo pipefail

cd "$(dirname "$0")"

make

exec ./build/handme "$@"

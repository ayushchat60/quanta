#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-$repo_root/build/debug}"
if [[ $# -gt 0 ]]; then shift; fi
ctest --test-dir "$build_dir" --output-on-failure --no-tests=error "$@"

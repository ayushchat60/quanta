#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_type="${1:-Debug}"
if [[ $# -gt 0 ]]; then shift; fi
case "$build_type" in
  Debug) default_dir="$repo_root/build/debug" ;;
  Release) default_dir="$repo_root/build/release" ;;
  *) echo "Usage: $0 [Debug|Release] [CMake options...]" >&2; exit 2 ;;
esac
build_dir="${QUANTA_BUILD_DIR:-$default_dir}"
if [[ -n "${CXX:-}" ]]; then
  compiler="$CXX"
elif [[ "$(uname -s)" == Darwin ]]; then
  compiler="$(brew --prefix llvm)/bin/clang++"
elif command -v clang++-18 >/dev/null; then
  compiler=clang++-18
else
  compiler=clang++
fi
cmake -S "$repo_root" -B "$build_dir" -DCMAKE_BUILD_TYPE="$build_type" \
  -DCMAKE_CXX_COMPILER="$compiler" "$@"
cmake --build "$build_dir" --parallel "${QUANTA_JOBS:-4}"

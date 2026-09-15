#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
mode="${1:---check}"
case "$mode" in
  --check) flags=(--dry-run --Werror) ;;
  --write) flags=(-i) ;;
  *) echo "Usage: $0 [--check|--write]" >&2; exit 2 ;;
esac
if [[ -n "${CLANG_FORMAT:-}" ]]; then
  formatter="$CLANG_FORMAT"
elif command -v clang-format-18 >/dev/null; then
  formatter=clang-format-18
elif [[ "$(uname -s)" == Darwin ]]; then
  formatter="$(brew --prefix llvm)/bin/clang-format"
else
  formatter=clang-format
fi
"$formatter" --version
# Search only project source trees, never dependency or build trees.
find include src app tests tools benchmarks -type f \( -name '*.h' -o -name '*.cpp' \) -print0 |
  xargs -0 "$formatter" "${flags[@]}"

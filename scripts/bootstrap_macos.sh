#!/usr/bin/env bash
set -euo pipefail
if [[ "$(uname -s)" != Darwin ]]; then
  echo "This script targets macOS with Homebrew." >&2; exit 1
fi
if ! command -v brew >/dev/null; then
  echo "Install Homebrew from https://brew.sh, then rerun this script." >&2; exit 1
fi
# Install missing dependencies only; upgrades remain an explicit developer choice.
for package in cmake ninja llvm apache-arrow; do
  if brew list --versions "$package" >/dev/null 2>&1; then
    echo "$package is installed"
  else
    brew install "$package"
  fi
done

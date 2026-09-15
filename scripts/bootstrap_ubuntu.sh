#!/usr/bin/env bash
set -euo pipefail
# Package source documented at https://arrow.apache.org/install/.
source /etc/os-release
if [[ "$ID" != ubuntu || "$VERSION_ID" != 24.04 ]]; then
  echo "This script targets Ubuntu 24.04." >&2; exit 1
fi
sudo apt-get update
sudo apt-get install -y ca-certificates curl cmake ninja-build clang-18 clang-format-18 clang-tidy-18 pkg-config
if ! dpkg-query -W -f='${Status}' apache-arrow-apt-source 2>/dev/null | grep -q 'install ok installed'; then
  package_dir="$(mktemp -d)"
  trap 'rm -rf "$package_dir"' EXIT
  curl --fail --location --retry 3 \
    https://packages.apache.org/artifactory/arrow/ubuntu/apache-arrow-apt-source-latest-noble.deb \
    --output "$package_dir/apache-arrow-apt-source.deb"
  sudo apt-get install -y "$package_dir/apache-arrow-apt-source.deb"
fi
sudo apt-get update
sudo apt-get install -y libarrow-dev libparquet-dev

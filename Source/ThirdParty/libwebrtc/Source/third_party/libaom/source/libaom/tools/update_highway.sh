#!/bin/bash

# Update third_party/highway to the latest version.

# Usage: (under libaom root directory)
# ./tools/update_highway.sh

set -e

highway_dir="$(pwd)/third_party/highway"
repo_url="https://github.com/google/highway"

git clone --depth 1 "$repo_url" "$highway_dir"

cd "${highway_dir}"

commit_hash=$(git rev-parse HEAD)

# Remove everything except ./hwy
find . -mindepth 1 \
  -not -path "./hwy" \
  -not -path "./hwy/*" \
  -not -name "LICENSE-BSD3" \
  -delete

# Remove tests/ directory
rm -rf hwy/tests/

# Remove markdown files
find . -name "*.md" -delete

# Remove cc files since we build highway header-only
find . -name "*.cc" -delete

# Update the include path
find ./hwy \( -name "*.c" -o -name "*.cc" -o -name "*.h" \) -print0 | \
  xargs -0 sed -i 's/#include "hwy\//#include "third_party\/highway\/hwy\//g'

find ./hwy \( -name "*.c" -o -name "*.cc" -o -name "*.h" \) -print0 | \
  xargs -0 sed -i \
  's/HWY_TARGET_INCLUDE "hwy\//HWY_TARGET_INCLUDE "third_party\/highway\/hwy\//g'

cat > "${highway_dir}/README.libaom" <<EOF
URL: $repo_url

Version: $commit_hash
License: BSD-3-clause clear
License File: LICENSE-BSD3

Description:
Highway is a C++ library that provides portable SIMD/vector intrinsics.

Local Changes:
Remove everything except hwy/ and LICENSE-BSD3
EOF

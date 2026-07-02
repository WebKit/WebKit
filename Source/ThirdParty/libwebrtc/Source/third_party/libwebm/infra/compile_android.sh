#!/bin/bash
# Copyright (c) 2021, Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#
#   * Neither the name of Google nor the names of its contributors may
#     be used to endorse or promote products derived from this software
#     without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

set -e
shopt -s inherit_errexit

LIBWEBM_ROOT="$(realpath "$(dirname "$0")/..")"
readonly LIBWEBM_ROOT
readonly WORKSPACE=${WORKSPACE:-"$(mktemp -d)"}

# shellcheck source=infra/common.sh
source "${LIBWEBM_ROOT}/infra/common.sh"

usage() {
  cat << EOF
Usage: $(basename "$0") APP_OPTIM APP_ABI
Options:
APP_OPTIM   supported build type (release, debug)
APP_ABI     supported application binary interface compilation: (armeabi-v7a,
            arm64-v8a, x86, x86_64)
Environment variables:
WORKSPACE   directory where the build is done
EOF
}

################################################################################
echo "Building libwebm for Android in ${WORKSPACE}"

if [[ ! -d "${WORKSPACE}" ]]; then
  log_err "${WORKSPACE} directory does not exist"
  exit 1
fi

APP_OPTIM=${1:?"not defined.$(
  echo
  usage
)"}
APP_ABI=${2:?"Application Binary Interface not defined.$(
  echo
  usage
)"}
BUILD_DIR="${WORKSPACE}/build-${APP_OPTIM}"

if ! command -v ndk-build 2> /dev/null; then
  log_err "unable to find ndk-build in PATH"
  exit 1
fi

make_build_dir "${BUILD_DIR}"
ndk-build -j2 NDK_PROJECT_PATH="${BUILD_DIR}" APP_OPTIM="${APP_OPTIM}" \
  APP_ABI="${APP_ABI}" APP_BUILD_SCRIPT="${LIBWEBM_ROOT}/Android.mk" \
  APP_STL="c++_static"

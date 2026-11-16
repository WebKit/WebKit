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
readonly WORKSPACE=${WORKSPACE:-"$(mktemp -d -t webm.XXX)"}

# shellcheck source=infra/common.sh
source "${LIBWEBM_ROOT}/infra/common.sh"

usage() {
  cat << EOF
Usage: compile.sh BUILD_TYPE TARGET
Options:
BUILD_TYPE  supported build type (static, static-debug)
TARGET      supported target platform compilation: (native, native-clang,
            clang-i686, i686-w64-mingw32, x86_64-w64-mingw32,
            native-Makefile.unix)
Environment variables:
WORKSPACE   directory where the build is done
EOF
}

#######################################
# Setup ccache for toolchain.
#######################################
setup_ccache() {
  if command -v ccache 2> /dev/null; then
    export CCACHE_CPP2=yes
    export PATH="/usr/lib/ccache:${PATH}"
  fi
}

################################################################################
echo "Building libwebm in ${WORKSPACE}"

if [[ ! -d "${WORKSPACE}" ]]; then
  log_err "${WORKSPACE} directory does not exist"
  exit 1
fi

BUILD_TYPE=${1:?"Build type not defined.$(
  echo
  usage
)"}
TARGET=${2:?"Target not defined.$(
  echo
  usage
)"}
BUILD_DIR="${WORKSPACE}/build-${BUILD_TYPE}"

trap cleanup EXIT
setup_ccache
make_build_dir "${BUILD_DIR}"

case "${TARGET}" in
  native-Makefile.unix)
    make -C "${LIBWEBM_ROOT}" -f Makefile.unix
    ;;
  *)
    opts=()
    case "${BUILD_TYPE}" in
      static) opts+=("-DCMAKE_BUILD_TYPE=Release") ;;
      *debug) opts+=("-DCMAKE_BUILD_TYPE=Debug") ;;
      *)
        log_err "${BUILD_TYPE} build type not supported"
        usage
        exit 1
        ;;
    esac

    TOOLCHAIN_FILE_FLAG="-DCMAKE_TOOLCHAIN_FILE=${LIBWEBM_ROOT}/build"
    case "${TARGET}" in
      native-clang) opts+=("-DCMAKE_CXX_COMPILER=clang++") ;;
      clang-i686)
        opts+=("-DCMAKE_CXX_COMPILER=clang++")
        opts+=("-DCMAKE_CXX_FLAGS=-m32")
        ;;
      native) ;; # No additional flags needed.
      i686-w64-mingw32)
        opts+=("${TOOLCHAIN_FILE_FLAG}/x86-mingw-gcc.cmake")
        ;;
      x86_64-w64-mingw32)
        opts+=("${TOOLCHAIN_FILE_FLAG}/x86_64-mingw-gcc.cmake")
        ;;
      *)
        log_err "${TARGET} TARGET not supported"
        usage
        exit 1
        ;;
    esac
    pushd "${BUILD_DIR}"
    cmake "${LIBWEBM_ROOT}" "${opts[@]}"
    make -j4 VERBOSE=1
    popd
    ;;
esac

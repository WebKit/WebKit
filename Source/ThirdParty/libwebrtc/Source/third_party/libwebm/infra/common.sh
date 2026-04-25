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

log_err() {
  echo "[$(date +'%Y-%m-%dT%H:%M:%S%z')]: $*" >&2
}

#######################################
# Create build directory. Build directory will be deleted if it exists.
# Outputs:
#   build dir path
# Returns:
#   mkdir result
#######################################
make_build_dir() {
  if [[ "$#" -ne 1 ]]; then
    return 1
  fi

  local build_dir
  build_dir="$1"
  [[ -d "${build_dir}" ]] && rm -rf "${build_dir}"
  mkdir -p "${build_dir}"
}

#######################################
# Cleanup files from the backup directory.
# Globals:
#   BUILD_DIR     build directory
#   LIBWEBM_ROOT  repository's root path
#######################################
cleanup() {
  # BUILD_DIR is not completely removed to allow for binary artifacts to be
  # extracted.
  find "${BUILD_DIR:?}" \( -name "*.[ao]" -o -name "*.l[ao]" \) -exec rm \
    -f {} +
  make -C "${LIBWEBM_ROOT:?}" -f Makefile.unix clean
}

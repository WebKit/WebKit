#!/bin/bash
# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ------------------------------------------------------------------------------
#
# tests for command lines (gain maps)

# Very verbose but useful for debugging.
set -ex

if [[ "$#" -ge 1 ]]; then
  # eval so that the passed in directory can contain variables.
  BINARY_DIR="$(eval echo "$1")"
else
  # Assume "tests" is the current directory.
  BINARY_DIR="$(pwd)/.."
fi
if [[ "$#" -ge 2 ]]; then
  TESTDATA_DIR="$(eval echo "$2")"
else
  TESTDATA_DIR="$(pwd)/data"
fi
if [[ "$#" -ge 3 ]]; then
  TMP_DIR="$(eval echo "$3")"
else
  TMP_DIR="$(mktemp -d)"
fi

AVIFENC="${BINARY_DIR}/avifenc"

# Input file paths.
INPUT_JPEG_GAINMAP="${TESTDATA_DIR}/paris_exif_xmp_gainmap_bigendian.jpg"
# Output file names.
ENCODED_FILE="avif_test_cmd_gainmap_encoded.avif"
OUT_MSG="avif_test_cmd_gainmap_out_msg.txt"

# Cleanup
cleanup() {
  pushd ${TMP_DIR}
    rm -- "${ENCODED_FILE}" "${OUT_MSG}"
  popd
}
trap cleanup EXIT

pushd ${TMP_DIR}
  # The default gain map quality is 60.
  "${AVIFENC}" -s 10 "${INPUT_JPEG_GAINMAP}" "${ENCODED_FILE}" > "${OUT_MSG}"
  grep " gain map quality \[60 " "${OUT_MSG}"
  size_q60=$(stat --printf="%s" "${ENCODED_FILE}")
  "${AVIFENC}" -s 10 --qgain-map 85 "${INPUT_JPEG_GAINMAP}" "${ENCODED_FILE}" > "${OUT_MSG}"
  grep " gain map quality \[85 " "${OUT_MSG}"
  size_q85=$(stat --printf="%s" "${ENCODED_FILE}")
  test "$size_q85" -gt "$size_q60"
  # With --ignore-gain-map, no gain map should be encoded
  "${AVIFENC}" -s 10 --qgain-map 85 --ignore-gain-map "${INPUT_JPEG_GAINMAP}" "${ENCODED_FILE}" > "${OUT_MSG}"
  grep "gain map quality" "${OUT_MSG}" && exit 1
  grep "Gain map *: Absent" "${OUT_MSG}"
popd

exit 0

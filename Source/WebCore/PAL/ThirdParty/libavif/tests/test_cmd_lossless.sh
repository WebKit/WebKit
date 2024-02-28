#!/bin/bash
# Copyright 2022 Google LLC
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
# tests for command lines (lossless)

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
AVIFDEC="${BINARY_DIR}/avifdec"
ARE_IMAGES_EQUAL="${BINARY_DIR}/tests/are_images_equal"

# Input file paths.
INPUT_PNG="${TESTDATA_DIR}/paris_icc_exif_xmp.png"
INPUT_GRAY_PNG="${TESTDATA_DIR}/kodim03_grayscale_gamma1.6.png"
# Output file names.
ENCODED_FILE="avif_test_cmd_lossless_encoded.avif"
DECODED_FILE="avif_test_cmd_lossless_decoded.png"
DECODED_FILE_LOSSLESS="avif_test_cmd_lossless_decoded_lossless.png"

# Cleanup
cleanup() {
  pushd ${TMP_DIR}
    rm -- "${ENCODED_FILE}" "${DECODED_FILE}" "${DECODED_FILE_LOSSLESS}"
  popd
}
trap cleanup EXIT

pushd ${TMP_DIR}
  # Generate test data.
  "${AVIFENC}" -s 8 "${INPUT_PNG}" -o "${ENCODED_FILE}"
  "${AVIFDEC}" "${ENCODED_FILE}" "${DECODED_FILE}"

  # Combining some arguments with lossless should fail.
  for option in "-y 420" "--min 0 --max 1" "-r limited" "--cicp 2/2/8"; do
    "${AVIFENC}" $option -s 10 -l "${DECODED_FILE}" -o "${ENCODED_FILE}" && exit 1
  done

  # Combining some arguments with lossless should work.
  for option in "-y 444" "--min 0 --max 0" "-r full"; do
    "${AVIFENC}" $option -s 10 -l "${DECODED_FILE}" -o "${ENCODED_FILE}"
  done

  # Lossless test. The decoded pixels should be the same as the original image.
  echo "Testing basic lossless"
  "${AVIFENC}" -s 10 -l "${INPUT_PNG}" -o "${ENCODED_FILE}"
  "${AVIFDEC}" "${ENCODED_FILE}" "${DECODED_FILE_LOSSLESS}"
  "${ARE_IMAGES_EQUAL}" "${INPUT_PNG}" "${DECODED_FILE_LOSSLESS}" 0

  # 400 test.
  echo "Testing 400 lossless"
  "${AVIFENC}" -y 400 -s 10 -l "${INPUT_GRAY_PNG}" -o "${ENCODED_FILE}"
  "${AVIFDEC}" "${ENCODED_FILE}" "${DECODED_FILE_LOSSLESS}"
  "${ARE_IMAGES_EQUAL}" "${INPUT_GRAY_PNG}" "${DECODED_FILE_LOSSLESS}" 0
popd

exit 0

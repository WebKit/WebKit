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
# tests for command lines

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

# Basic calls.
"${AVIFENC}" --version
"${AVIFDEC}" --version

# Input file paths.
INPUT_Y4M="${TESTDATA_DIR}/kodim03_yuv420_8bpc.y4m"
INPUT_PNG="${TESTDATA_DIR}/paris_icc_exif_xmp.png"
INPUT_JPG="${TESTDATA_DIR}/paris_exif_xmp_icc.jpg"
# Output file names.
ENCODED_FILE="avif_test_cmd_encoded.avif"
ENCODED_FILE_NO_METADATA="avif_test_cmd_encoded_no_metadata.avif"
ENCODED_FILE_WITH_DASH="-avif_test_cmd_encoded.avif"
DECODED_FILE="avif_test_cmd_decoded.png"
DECODED_FILE_LOSSLESS="avif_test_cmd_decoded_lossless.png"

# Cleanup
cleanup() {
  pushd ${TMP_DIR}
    rm -- "${ENCODED_FILE}" "${ENCODED_FILE_NO_METADATA}" \
          "${ENCODED_FILE_WITH_DASH}" "${DECODED_FILE}" "${DECODED_FILE_LOSSLESS}"
  popd
}
trap cleanup EXIT

pushd ${TMP_DIR}
  # Lossy test. The decoded pixels should be different from the original image.
  echo "Testing basic lossy"
  "${AVIFENC}" -s 8 "${INPUT_Y4M}" -o "${ENCODED_FILE}"
  "${AVIFDEC}" "${ENCODED_FILE}" "${DECODED_FILE}"
  "${ARE_IMAGES_EQUAL}" "${INPUT_Y4M}" "${DECODED_FILE}" 0 && exit 1

  # Lossless test. The decoded pixels should be the same as the original image.
  echo "Testing basic lossless"
  # TODO(yguyon): Make this test pass with INPUT_PNG instead of DECODED_FILE.
  "${AVIFENC}" -s 10 -l "${DECODED_FILE}" -o "${ENCODED_FILE}"
  "${AVIFDEC}" "${ENCODED_FILE}" "${DECODED_FILE_LOSSLESS}"
  "${ARE_IMAGES_EQUAL}" "${DECODED_FILE}" "${DECODED_FILE_LOSSLESS}" 0

  # Argument parsing test with filenames starting with a dash.
  echo "Testing arguments"
  "${AVIFENC}" -s 10 "${INPUT_PNG}" -- "${ENCODED_FILE_WITH_DASH}"
  "${AVIFDEC}" --info  -- "${ENCODED_FILE_WITH_DASH}"
  # Passing a filename starting with a dash without using -- should fail.
  "${AVIFENC}" -s 10 "${INPUT_PNG}" "${ENCODED_FILE_WITH_DASH}" && exit 1
  "${AVIFDEC}" --info "${ENCODED_FILE_WITH_DASH}" && exit 1

  # Metadata test.
  echo "Testing metadata enc/dec"
  for INPUT in "${INPUT_PNG}" "${INPUT_JPG}"; do
    "${AVIFENC}" "${INPUT}" -o "${ENCODED_FILE}"
    # Ignoring a metadata chunk should produce a different output file.
    "${AVIFENC}" "${INPUT}" -o "${ENCODED_FILE_NO_METADATA}" --ignore-icc
    cmp "${ENCODED_FILE}" "${ENCODED_FILE_NO_METADATA}" && exit 1
    "${AVIFENC}" "${INPUT}" -o "${ENCODED_FILE_NO_METADATA}" --ignore-exif
    cmp "${ENCODED_FILE}" "${ENCODED_FILE_NO_METADATA}" && exit 1
    "${AVIFENC}" "${INPUT}" -o "${ENCODED_FILE_NO_METADATA}" --ignore-xmp
    cmp "${ENCODED_FILE}" "${ENCODED_FILE_NO_METADATA}" && exit 1
  done
popd

exit 0

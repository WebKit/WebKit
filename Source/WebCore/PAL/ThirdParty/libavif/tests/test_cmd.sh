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
INPUT_UTF8_Y4M="🐾.y4m"
# Output file names.
ENCODED_FILE="avif_test_cmd_encoded.avif"
ENCODED_UTF8_FILE="🐾.avif"
ENCODED_FILE_REFERENCE="avif_test_cmd_encoded_ref.avif"
ENCODED_FILE_WITH_DASH="-avif_test_cmd_encoded.avif"
DECODED_FILE="avif_test_cmd_decoded.png"
DECODED_UTF8_FILE="😀.png"
OUT_MSG="avif_test_cmd_out_msg.txt"

# Cleanup
cleanup() {
  pushd ${TMP_DIR}
    rm -f -- "${ENCODED_FILE}" "${ENCODED_FILE_WITH_DASH}" "${DECODED_FILE}" "${OUT_MSG}"
  popd
}
trap cleanup EXIT

pushd ${TMP_DIR}
  # Lossy test. The decoded pixels should be different from the original image.
  echo "Testing basic lossy"
  "${AVIFENC}" -s 8 "${INPUT_Y4M}" -o "${ENCODED_FILE}"
  "${AVIFDEC}" "${ENCODED_FILE}" "${DECODED_FILE}"
  "${ARE_IMAGES_EQUAL}" "${INPUT_Y4M}" "${DECODED_FILE}" 0 && exit 1
  cp ${INPUT_Y4M} ${INPUT_UTF8_Y4M}
  "${AVIFENC}" -s 8 "${INPUT_UTF8_Y4M}" -o "${ENCODED_UTF8_FILE}"
  "${AVIFDEC}" "${ENCODED_UTF8_FILE}" "${DECODED_UTF8_FILE}"
  RET=0
  "${ARE_IMAGES_EQUAL}" "${INPUT_UTF8_Y4M}" "${DECODED_UTF8_FILE}" 0 || RET=$?
  if [[ ${RET} -ne 1 ]]; then
    exit 1
  fi

  # Argument parsing test with filenames starting with a dash.
  echo "Testing arguments"
  "${AVIFENC}" -s 10 "${INPUT_Y4M}" -- "${ENCODED_FILE_WITH_DASH}"
  "${AVIFDEC}" --info  -- "${ENCODED_FILE_WITH_DASH}"
  # Passing a filename starting with a dash without using -- should fail.
  "${AVIFENC}" -s 10 "${INPUT_Y4M}" "${ENCODED_FILE_WITH_DASH}" && exit 1
  "${AVIFDEC}" --info "${ENCODED_FILE_WITH_DASH}" && exit 1

  # Option update handling test
  # Passing non-update option before input should not print warning.
  "${AVIFENC}" -s 10 -q 85 "${INPUT_Y4M}" "${ENCODED_FILE_REFERENCE}" 2> "${OUT_MSG}"
  grep "WARNING: -q" "${OUT_MSG}" && exit 1
  grep "WARNING: Trailing options" "${OUT_MSG}" && exit 1
  # Passing non-update option after input should print warning.
  "${AVIFENC}" -s 10 "${INPUT_Y4M}" "${ENCODED_FILE}" -q 85 2> "${OUT_MSG}"
  grep "WARNING: -q" "${OUT_MSG}"
  cmp -s "${ENCODED_FILE_REFERENCE}" "${ENCODED_FILE}"
  # Passing non-update option after input but before positional output should also print warning.
  "${AVIFENC}" -s 10 "${INPUT_Y4M}" -q 85 "${ENCODED_FILE}" 2> "${OUT_MSG}"
  grep "WARNING: -q" "${OUT_MSG}"
  cmp -s "${ENCODED_FILE_REFERENCE}" "${ENCODED_FILE}"
  # Passing update option after input should print warning, and has no effect.
  "${AVIFENC}" -s 10 "${INPUT_Y4M}" "${ENCODED_FILE}" -q:u 85 2> "${OUT_MSG}"
  grep "WARNING: Trailing options" "${OUT_MSG}"
  cmp -s "${ENCODED_FILE_REFERENCE}" "${ENCODED_FILE}" && exit 1
  # Passing update option after input but before positional output should also print warning, and has no effect.
  "${AVIFENC}" -s 10 "${INPUT_Y4M}" -q:u 85 "${ENCODED_FILE}" 2> "${OUT_MSG}"
  grep "WARNING: Trailing options" "${OUT_MSG}"
  cmp -s "${ENCODED_FILE_REFERENCE}" "${ENCODED_FILE}" && exit 1

  # --min and --max must be both specified.
  "${AVIFENC}" -s 10 --min 24 "${INPUT_Y4M}" "${ENCODED_FILE}" && exit 1
  "${AVIFENC}" -s 10 --max 26 "${INPUT_Y4M}" "${ENCODED_FILE}" && exit 1
  # --minalpha and --maxalpha must be both specified.
  "${AVIFENC}" -s 10 --minalpha 0 "${INPUT_PNG}" "${ENCODED_FILE}" && exit 1
  "${AVIFENC}" -s 10 --maxalpha 0 "${INPUT_PNG}" "${ENCODED_FILE}" && exit 1

  # The default quality is 60. The default alpha quality is 100 (lossless).
  "${AVIFENC}" -s 10 "${INPUT_Y4M}" "${ENCODED_FILE}" > "${OUT_MSG}"
  grep " color quality \[60 " "${OUT_MSG}"
  grep " alpha quality \[100 " "${OUT_MSG}"
  "${AVIFENC}" -s 10 -q 85 "${INPUT_Y4M}" "${ENCODED_FILE}" > "${OUT_MSG}"
  grep " color quality \[85 " "${OUT_MSG}"
  grep " alpha quality \[100 " "${OUT_MSG}"
  # The average of 15 and 25 is 20. Quantizer 20 maps to quality 68.
  "${AVIFENC}" -s 10 --min 15 --max 25 "${INPUT_Y4M}" "${ENCODED_FILE}" > "${OUT_MSG}"
  grep " color quality \[68 " "${OUT_MSG}"
  grep " alpha quality \[100 " "${OUT_MSG}"
  "${AVIFENC}" -s 10 -q 65 --min 15 --max 25 "${INPUT_Y4M}" "${ENCODED_FILE}" > "${OUT_MSG}"
  grep " color quality \[65 " "${OUT_MSG}"
  grep " alpha quality \[100 " "${OUT_MSG}"
popd

exit 0

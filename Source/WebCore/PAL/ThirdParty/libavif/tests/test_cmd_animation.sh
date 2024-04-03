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

set -ex

if [[ "$#" -ge 1 ]]; then
  BINARY_DIR="$(eval echo "$1")"
else
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
INPUT_Y4M_0="${TESTDATA_DIR}/kodim03_yuv420_8bpc.y4m"
INPUT_Y4M_1="${TESTDATA_DIR}/kodim23_yuv420_8bpc.y4m"
INPUT_BAD_DIMENSIONS="${TESTDATA_DIR}/paris_exif_xmp_icc.jpg"
# Output file names.
ENCODED_FILE="avif_test_cmd_animation_encoded.avif"
DECODED_FILE="avif_test_cmd_animation_decoded.y4m"
ERROR_MSG="avif_test_cmd_animation_error_msg.txt"

cleanup() {
  pushd ${TMP_DIR}
    rm -- "${ENCODED_FILE}" "${DECODED_FILE}" "${ERROR_MSG}"
  popd
}
trap cleanup EXIT

pushd ${TMP_DIR}
  # Lossy test.
  "${AVIFENC}" -s 8 "${INPUT_Y4M_0}" "${INPUT_Y4M_1}" -o "${ENCODED_FILE}"
  "${AVIFDEC}" "${ENCODED_FILE}" "${DECODED_FILE}"
  # The are_images_equal binary only reads the first frame of each input file.
  "${ARE_IMAGES_EQUAL}" "${INPUT_Y4M_0}" "${DECODED_FILE}" 0 && exit 1

  # Lossless test.
  "${AVIFENC}" -s 8 "${INPUT_Y4M_0}" "${INPUT_Y4M_1}" -q 100 -o "${ENCODED_FILE}"
  "${AVIFDEC}" "${ENCODED_FILE}" "${DECODED_FILE}"
  "${ARE_IMAGES_EQUAL}" "${INPUT_Y4M_0}" "${DECODED_FILE}" 0

  # All input frames must have the same size.
  "${AVIFENC}" "${INPUT_Y4M_0}" "${INPUT_BAD_DIMENSIONS}" -o "${ENCODED_FILE}" \
    2> "${ERROR_MSG}" && exit 1
  grep "dimensions mismatch" "${ERROR_MSG}"

  # Output should be larger if second frame is set to higher quality.
  "${AVIFENC}" -s 8 -q 60 "${INPUT_Y4M_0}" "${INPUT_Y4M_1}" -o "${ENCODED_FILE}"
  "${AVIFDEC}" "${ENCODED_FILE}" "${DECODED_FILE}"
  Q60_FILE_SIZE=$(wc -c < "${ENCODED_FILE}")
  "${AVIFENC}" -s 8 -q 60 "${INPUT_Y4M_0}" -q:u 100 "${INPUT_Y4M_1}" -o "${ENCODED_FILE}"
  "${AVIFDEC}" "${ENCODED_FILE}" "${DECODED_FILE}"
  Q60_Q100_FILE_SIZE=$(wc -c < "${ENCODED_FILE}")
  [[ ${Q60_FILE_SIZE} -lt ${Q60_Q100_FILE_SIZE} ]] || exit 1

popd

exit 0

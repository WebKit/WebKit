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
# tests for command lines (--target-size)

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

# Input file paths.
INPUT_Y4M="${TESTDATA_DIR}/kodim03_yuv420_8bpc.y4m"
# Output file names.
ENCODED_FILE="avif_test_cmd_targetsize_encoded.avif"
DECODED_SMALLEST_FILE="avif_test_cmd_targetsize_decoded_smallest.png"
DECODED_BIGGEST_FILE="avif_test_cmd_targetsize_decoded_biggest.png"

# Cleanup
cleanup() {
  pushd ${TMP_DIR}
    rm -- "${ENCODED_FILE}" "${DECODED_SMALLEST_FILE}" "${DECODED_BIGGEST_FILE}"
  popd
}
trap cleanup EXIT

pushd ${TMP_DIR}
  "${AVIFENC}" -s 8 "${INPUT_Y4M}" -o "${ENCODED_FILE}"
  DEFAULT_QUALITY_FILE_SIZE=$(wc -c < "${ENCODED_FILE}")

  "${AVIFENC}" -s 8 "${INPUT_Y4M}" -o "${ENCODED_FILE}" --target-size 0
  "${AVIFDEC}" "${ENCODED_FILE}" "${DECODED_SMALLEST_FILE}"
  SMALLEST_FILE_SIZE=$(wc -c < "${ENCODED_FILE}")

  "${AVIFENC}" -s 8 "${INPUT_Y4M}" -o "${ENCODED_FILE}" --target-size 999999999
  "${AVIFDEC}" "${ENCODED_FILE}" "${DECODED_BIGGEST_FILE}"
  BIGGEST_FILE_SIZE=$(wc -c < "${ENCODED_FILE}")

  [[ ${SMALLEST_FILE_SIZE} -lt ${DEFAULT_QUALITY_FILE_SIZE} ]] || exit 1
  [[ ${DEFAULT_QUALITY_FILE_SIZE} -lt ${BIGGEST_FILE_SIZE} ]] || exit 1

  # Check that min/max file sizes match lowest/highest qualities.
  "${AVIFENC}" -s 8 "${INPUT_Y4M}" -o "${ENCODED_FILE}" -q 0
  [[ $(wc -c < "${ENCODED_FILE}") -eq ${SMALLEST_FILE_SIZE} ]] || exit 1
  "${AVIFENC}" -s 8 "${INPUT_Y4M}" -o "${ENCODED_FILE}" -q 100
  [[ $(wc -c < "${ENCODED_FILE}") -eq ${BIGGEST_FILE_SIZE} ]] || exit 1
  # Negative test.
  [[ $(wc -c < "${ENCODED_FILE}") -eq ${SMALLEST_FILE_SIZE} ]] && exit 1

  # Same as above but with a grid made of two tiles.
  TILE0="${DECODED_SMALLEST_FILE}"
  TILE1="${DECODED_BIGGEST_FILE}"

  "${AVIFENC}" -s 9 --grid 2x1 "${TILE0}" "${TILE1}" -o "${ENCODED_FILE}"
  DEFAULT_QUALITY_FILE_SIZE=$(wc -c < "${ENCODED_FILE}")

  "${AVIFENC}" -s 9 --grid 2x1 "${TILE0}" "${TILE1}" -o "${ENCODED_FILE}" --target-size 0
  SMALLEST_FILE_SIZE=$(wc -c < "${ENCODED_FILE}")

  "${AVIFENC}" -s 9 --grid 2x1 "${TILE0}" "${TILE1}" -o "${ENCODED_FILE}" --target-size 999999999
  BIGGEST_FILE_SIZE=$(wc -c < "${ENCODED_FILE}")

  [[ ${SMALLEST_FILE_SIZE} -lt ${DEFAULT_QUALITY_FILE_SIZE} ]] || exit 1
  [[ ${DEFAULT_QUALITY_FILE_SIZE} -lt ${BIGGEST_FILE_SIZE} ]] || exit 1

  # The remaining tests in this file use animations that may trigger segmentation faults
  # with libaom versions older than 3.6.0. Skip the tests if that is the case.
  # TODO(yguyon): Investigate.
  # The grep and cut commands are not pretty but seem to work on most platforms.
  if "${AVIFENC}" -V | grep -o "aom" --quiet; then
    AOM_VERSION=$("${AVIFENC}" -V | grep -o "aom.*[0-9]*\.")        # "aom [enc/dec]:X.Y."
    AOM_VERSION=$(echo "${AOM_VERSION}" | cut -d ":" -f 2)          # "X.Y" maybe prepended by a 'v'
    AOM_VERSION=$(echo "${AOM_VERSION}" | grep -o "[0-9]*\.[0-9]*") # "X.Y"
    AOM_MAJOR_VERSION=$(echo "${AOM_VERSION}" | cut -d "." -f 1)
    AOM_MINOR_VERSION=$(echo "${AOM_VERSION}" | cut -d "." -f 2)
    [[ ${AOM_MAJOR_VERSION} -ge 3 ]] || exit 0 # older than 3.0.0
    [[ ${AOM_MAJOR_VERSION} -ge 4 ]] || [[ ${AOM_MINOR_VERSION} -ge 6 ]] || exit 0 # older than 3.5.0
  fi

  # Same as above but with an animation made of two frames.
  FRAME0="${DECODED_SMALLEST_FILE}"
  FRAME1="${DECODED_BIGGEST_FILE}"

  "${AVIFENC}" -s 9 "${FRAME0}" "${FRAME1}" -o "${ENCODED_FILE}"
  DEFAULT_QUALITY_FILE_SIZE=$(wc -c < "${ENCODED_FILE}")

  "${AVIFENC}" -s 9 "${FRAME0}" "${FRAME1}" -o "${ENCODED_FILE}" --target-size 0
  SMALLEST_FILE_SIZE=$(wc -c < "${ENCODED_FILE}")

  "${AVIFENC}" -s 9 "${FRAME0}" "${FRAME1}" -o "${ENCODED_FILE}" --target-size 999999999
  BIGGEST_FILE_SIZE=$(wc -c < "${ENCODED_FILE}")

  [[ ${SMALLEST_FILE_SIZE} -lt ${DEFAULT_QUALITY_FILE_SIZE} ]] || exit 1
  [[ ${DEFAULT_QUALITY_FILE_SIZE} -lt ${BIGGEST_FILE_SIZE} ]] || exit 1
popd

exit 0

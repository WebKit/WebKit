#!/bin/sh
## Copyright (c) 2016, Alliance for Open Media. All rights reserved.
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
## This file tests the libaom decode_to_md5 example. To add new tests to this
## file, do the following:
##   1. Write a shell function (this is your test).
##   2. Add the function to decode_to_md5_tests (on a new line).
##
. $(dirname $0)/tools_common.sh

# Environment check: Make sure input is available:
#   $AV1_IVF_FILE is required.
decode_to_md5_verify_environment() {
  if [ "$(av1_encode_available)" != "yes" ] && [ ! -e "${AV1_IVF_FILE}" ]; then
    return 1
  fi
}

# Runs decode_to_md5 on $1 and captures the md5 sum for the final frame. $2 is
# interpreted as codec name and used solely to name the output file. $3 is the
# expected md5 sum: It must match that of the final frame.
decode_to_md5() {
  local decoder="$(aom_tool_path decode_to_md5)"
  local input_file="$1"
  local codec="$2"
  local expected_md5="$3"
  local output_file="${AOM_TEST_OUTPUT_DIR}/decode_to_md5_${codec}"

  if [ ! -x "${decoder}" ]; then
    elog "${decoder} does not exist or is not executable."
    return 1
  fi

  eval "${AOM_TEST_PREFIX}" "${decoder}" "${input_file}" "${output_file}" \
      ${devnull} || return 1

  [ -e "${output_file}" ] || return 1

  local md5_last_frame="$(tail -n1 "${output_file}" | awk '{print $1}')"
  local actual_md5="$(echo "${md5_last_frame}" | awk '{print $1}')"
  if [ "${actual_md5}" = "${expected_md5}" ]; then
    return 0
  else
    elog "MD5 mismatch:"
    elog "Expected: ${expected_md5}"
    elog "Actual: ${actual_md5}"
    return 1
  fi
}

DISABLED_decode_to_md5_av1() {
  # expected MD5 sum for the last frame.
  local expected_md5="567dd6d4b7a7170edddbf58bbcc3aff1"
  local file="${AV1_IVF_FILE}"

  # TODO(urvang): Check in the encoded file (like libvpx does) to avoid
  # encoding every time.
  if [ "$(av1_decode_available)" = "yes" ]; then
    if [ ! -e "${AV1_IVF_FILE}" ]; then
      file="${AOM_TEST_OUTPUT_DIR}/test_encode.ivf"
      encode_yuv_raw_input_av1 "${file}" --ivf || return 1
    fi
    decode_to_md5 "${file}" "av1" "${expected_md5}"
  fi
}

# TODO(tomfinegan): Enable when the bitstream stabilizes.
decode_to_md5_tests="DISABLED_decode_to_md5_av1"

run_tests decode_to_md5_verify_environment "${decode_to_md5_tests}"

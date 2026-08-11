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
## This file tests the libaom decode_with_drops example. To add new tests to
## this file, do the following:
##   1. Write a shell function (this is your test).
##   2. Add the function to decode_with_drops_tests (on a new line).
##
. $(dirname $0)/tools_common.sh

# Environment check: Make sure input is available:
#   $AV1_IVF_FILE is required.
decode_with_drops_verify_environment() {
  if [ "$(av1_encode_available)" != "yes" ] && [ ! -e "${AV1_IVF_FILE}" ]; then
    return 1
  fi
}

# Runs decode_with_drops on $1, $2 is interpreted as codec name and used solely
# to name the output file. $3 is the drop mode, and is passed directly to
# decode_with_drops.
decode_with_drops() {
  local decoder="$(aom_tool_path decode_with_drops)"
  local input_file="$1"
  local codec="$2"
  local output_file="${AOM_TEST_OUTPUT_DIR}/decode_with_drops_${codec}"
  local drop_mode="$3"

  if [ ! -x "${decoder}" ]; then
    elog "${decoder} does not exist or is not executable."
    return 1
  fi

  eval "${AOM_TEST_PREFIX}" "${decoder}" "${input_file}" "${output_file}" \
      "${drop_mode}" ${devnull} || return 1

  [ -e "${output_file}" ] || return 1
}


# Decodes $AV1_IVF_FILE while dropping frames, twice: once in sequence mode,
# and once in pattern mode.
DISABLED_decode_with_drops_av1() {
  if [ "$(av1_decode_available)" = "yes" ]; then
    local file="${AV1_IVF_FILE}"
    if [ ! -e "${AV1_IVF_FILE}" ]; then
      file="${AOM_TEST_OUTPUT_DIR}/test_encode.ivf"
      encode_yuv_raw_input_av1 "${file}" --ivf || return 1
    fi
    # Drop frames 3 and 4.
    decode_with_drops "${file}" "av1" "3-4" || return 1

    # Test pattern mode: Drop 3 of every 4 frames.
    decode_with_drops "${file}" "av1" "3/4" || return 1
  fi
}

# TODO(yaowu): Disable this test as trailing_bit check is expected to fail
decode_with_drops_tests="DISABLED_decode_with_drops_av1"

run_tests decode_with_drops_verify_environment "${decode_with_drops_tests}"

#!/bin/sh
## Copyright (c) 2016, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
## This file tests the libaom simple_decoder example code. To add new tests to
## this file, do the following:
##   1. Write a shell function (this is your test).
##   2. Add the function to simple_decoder_tests (on a new line).
##
. $(dirname $0)/tools_common.sh

# Environment check: Make sure input is available:
simple_decoder_verify_environment() {
  if [ ! "$(av1_encode_available)" = "yes" ] && [ ! -e "${AV1_IVF_FILE}" ]; then
    return 1
  fi
}

# Runs simple_decoder using $1 as input file. $2 is the codec name, and is used
# solely to name the output file.
simple_decoder() {
  local decoder="$(aom_tool_path simple_decoder)"
  local input_file="$1"
  local codec="$2"
  local output_file="${AOM_TEST_OUTPUT_DIR}/simple_decoder_${codec}.raw"

  if [ ! -x "${decoder}" ]; then
    elog "${decoder} does not exist or is not executable."
    return 1
  fi

  eval "${AOM_TEST_PREFIX}" "${decoder}" "${input_file}" "${output_file}" \
      ${devnull} || return 1

  [ -e "${output_file}" ] || return 1
}

simple_decoder_av1() {
  if [ "$(av1_decode_available)" = "yes" ]; then
    if [ ! -e "${AV1_IVF_FILE}" ]; then
      local file="${AOM_TEST_OUTPUT_DIR}/test_encode.ivf"
      encode_yuv_raw_input_av1 "${file}" --ivf
      simple_decoder "${file}" av1 || return 1
    else
      simple_decoder "${AV1_IVF_FILE}" av1 || return 1
    fi
  fi
}

simple_decoder_tests="simple_decoder_av1"

run_tests simple_decoder_verify_environment "${simple_decoder_tests}"

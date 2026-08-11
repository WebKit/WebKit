#!/bin/sh
## Copyright (c) 2026, Alliance for Open Media. All rights reserved.
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
## This file tests the libaom low-complexity decode mode using simple_encoder
## and simple_decoder example. The decoder outputs the instruction counts that
## can be used for decoder speed monitoring.
##
. $(dirname $0)/tools_common.sh

# Environment check: $infile is required.
low_complexity_test_verify_environment() {
  if [ ! "$(av1_encode_available)" = "yes" ] || \
    [ ! "$(av1_decode_available)" = "yes" ]; then
    echo "Encoder and decoder needs to be available."
    return 1
  fi

  local infile="${LIBAOM_TEST_DATA_PATH}/SDR_Sports_6mug_608p_30fps_90f.yuv"
  if [ ! -e "${infile}" ]; then
    echo "Libaom test data must exist in LIBAOM_TEST_DATA_PATH."
    return 1
  fi

  command -v perf >/dev/null 2>&1
  if [ $? -ne 0 ]; then
    echo "Perf does not exist. Please install it."
    return 1
  fi
}

# Runs libaom low-complexity decode mode test
low_complexity_test() {
  local img_width=608
  local img_height=1080

  # Encode in low-complexity decode mode
  local encoder="${LIBAOM_BIN_PATH}/low_complexity_mode_encoder${AOM_TEST_EXE_SUFFIX}"
  local input_file="${LIBAOM_TEST_DATA_PATH}/SDR_Sports_6mug_608p_30fps_90f.yuv"
  local ivf_file="${AOM_TEST_OUTPUT_DIR}/low_complexity_mode_encoder.bin"

  if [ ! -x "${encoder}" ]; then
    elog "${encoder} does not exist or is not executable."
    return 1
  fi

  echo "The LC mode encoder is started."
  local enc_cmd="$(echo "${AOM_TEST_PREFIX}" "${encoder}" av1 "${img_width}" \
                 "${img_height}" "${input_file}" "${ivf_file}" 150 90 "\
                 "${devnull})"
  echo "$enc_cmd"
  eval "$enc_cmd" || return 1

  [ -e "${ivf_file}" ] || return 1
  echo "The LC mode encoder is completed."

  # Use simple_decoder to decode the generated bitstream and collect the
  # instruction counts.
  local decoder="${LIBAOM_BIN_PATH}/simple_decoder${AOM_TEST_EXE_SUFFIX}"
  local perfstat_file="${AOM_TEST_OUTPUT_DIR}/perfstat.txt"
  local perf_prefix="perf stat --no-big-num -e instructions:u -o ${perfstat_file}"
  local output_file="${AOM_TEST_OUTPUT_DIR}/low_complexity_mode_encoder.out"

  if [ ! -x "${decoder}" ]; then
    elog "${decoder} does not exist or is not executable."
    return 1
  fi

  local dec_cmd="$(echo "${perf_prefix}" "${AOM_TEST_PREFIX}" "${decoder}" \
                 "${ivf_file}" "${output_file}" ${devnull})"
  echo "$dec_cmd"
  eval "$dec_cmd" || return 1

  # Get perf user instruction count from perf output
  local instruction_count="$(awk '/instructions:u/ {print $1}' \
                           "${perfstat_file}")"
  echo ${instruction_count}
}

low_complexity_decode_mode_test="low_complexity_test"

run_tests low_complexity_test_verify_environment \
  "${low_complexity_decode_mode_test}"

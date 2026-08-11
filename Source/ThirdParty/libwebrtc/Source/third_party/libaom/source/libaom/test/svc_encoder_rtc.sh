#!/bin/sh
## Copyright (c) 2023, Alliance for Open Media. All rights reserved.
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##

. $(dirname $0)/tools_common.sh

# Environment check: $YUV_RAW_INPUT is required.
svc_encoder_verify_environment() {
  if [ ! -e "${YUV_RAW_INPUT}" ]; then
    echo "Libaom test data must exist in LIBAOM_TEST_DATA_PATH."
    return 1
  fi
}

common_flags="-k 10000"
common_flags="${common_flags} --max-q=63"
common_flags="${common_flags} --error-resilient=0"

# Runs svc_encoder_rtc with 1 spatial layer 3 temporal layers.
svc_encoder_s1_t3() {
  local encoder="${LIBAOM_BIN_PATH}/svc_encoder_rtc${AOM_TEST_EXE_SUFFIX}"
  local output_file="${AOM_TEST_OUTPUT_DIR}/svc_encoder_rtc"

  if [ ! -x "${encoder}" ]; then
    elog "${encoder} does not exist or is not executable."
    return 1
  fi

  eval "${AOM_TEST_PREFIX}" "${encoder}" "${common_flags}" \
      "--width=${YUV_RAW_INPUT_WIDTH}" \
      "--height=${YUV_RAW_INPUT_HEIGHT}" \
      "-lm 2" \
      "--speed=8" \
      "--target-bitrate=400" \
      "--bitrates=220,300,400" \
      "--spatial-layers=1" \
      "--temporal-layers=3" \
      "--timebase=1/30" \
      "${YUV_RAW_INPUT}" \
      "-o ${output_file}" \
      ${devnull} || return 1

  [ -e "${output_file}" ] || return 1
}

# Runs svc_encoder_rtc with 1 spatial layer 2 temporal layers with
# speed 10.
svc_encoder_s1_t2() {
  local encoder="${LIBAOM_BIN_PATH}/svc_encoder_rtc${AOM_TEST_EXE_SUFFIX}"
  local output_file="${AOM_TEST_OUTPUT_DIR}/svc_encoder_rtc"

  if [ ! -x "${encoder}" ]; then
    elog "${encoder} does not exist or is not executable."
    return 1
  fi

  eval "${AOM_TEST_PREFIX}" "${encoder}" "${common_flags}" \
      "--width=${YUV_RAW_INPUT_WIDTH}" \
      "--height=${YUV_RAW_INPUT_HEIGHT}" \
      "-lm 1" \
      "--speed=10" \
      "--target-bitrate=400" \
      "--bitrates=220,400" \
      "--spatial-layers=1" \
      "--temporal-layers=2" \
      "--timebase=1/30" \
      "${YUV_RAW_INPUT}" \
      "-o ${output_file}" \
      ${devnull} || return 1

  [ -e "${output_file}" ] || return 1
}

# Runs svc_encoder_rtc with 2 spatial layers 1 temporal layer, specifying
# one input file per layer (although it's the same file twice).
svc_encoder_s2_t1() {
  local encoder="${LIBAOM_BIN_PATH}/svc_encoder_rtc${AOM_TEST_EXE_SUFFIX}"
  local output_file="${AOM_TEST_OUTPUT_DIR}/svc_encoder_rtc"

  if [ ! -x "${encoder}" ]; then
    elog "${encoder} does not exist or is not executable."
    return 1
  fi

  eval "${AOM_TEST_PREFIX}" "${encoder}" "${common_flags}" \
      "--width=${YUV_RAW_INPUT_WIDTH}" \
      "--height=${YUV_RAW_INPUT_HEIGHT}" \
      "-lm 5" \
      "--speed=8" \
      "--target-bitrate=400" \
      "--bitrates=100,300" \
      "--spatial-layers=2" \
      "--temporal-layers=1" \
      "--timebase=1/30" \
      "${YUV_RAW_INPUT}" \
      "${YUV_RAW_INPUT}" \
      "-o ${output_file}" \
      ${devnull} || return 1

  [ -e "${output_file}" ] || return 1
}

if [ "$(av1_encode_available)" = "yes" ]; then
  svc_encoder_rtc_tests="svc_encoder_s1_t3
                         svc_encoder_s1_t2
                         svc_encoder_s2_t1"
  run_tests svc_encoder_verify_environment "${svc_encoder_rtc_tests}"
fi

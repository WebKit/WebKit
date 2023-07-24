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
## This file tests aomenc using hantro_collage_w352h288.yuv as input. To add
## new tests to this file, do the following:
##   1. Write a shell function (this is your test).
##   2. Add the function to aomenc_tests (on a new line).
##
. $(dirname $0)/tools_common.sh

# Environment check: Make sure input is available.
aomenc_verify_environment() {
  if [ ! -e "${YUV_RAW_INPUT}" ]; then
    elog "The file ${YUV_RAW_INPUT##*/} must exist in LIBAOM_TEST_DATA_PATH."
    return 1
  fi
  if [ "$(aomenc_can_encode_av1)" = "yes" ]; then
    if [ ! -e "${Y4M_NOSQ_PAR_INPUT}" ]; then
      elog "The file ${Y4M_NOSQ_PAR_INPUT##*/} must exist in"
      elog "LIBAOM_TEST_DATA_PATH."
      return 1
    fi
  fi
  if [ -z "$(aom_tool_path aomenc)" ]; then
    elog "aomenc not found. It must exist in LIBAOM_BIN_PATH or its parent."
    return 1
  fi
}

aomenc_can_encode_av1() {
  if [ "$(av1_encode_available)" = "yes" ]; then
    echo yes
  fi
}

# Utilities that echo aomenc input file parameters.
y4m_input_non_square_par() {
  echo ""${Y4M_NOSQ_PAR_INPUT}""
}

y4m_input_720p() {
  echo ""${Y4M_720P_INPUT}""
}

# Wrapper function for running aomenc with pipe input. Requires that
# LIBAOM_BIN_PATH points to the directory containing aomenc. $1 is used as the
# input file path and shifted away. All remaining parameters are passed through
# to aomenc.
aomenc_pipe() {
  local encoder="$(aom_tool_path aomenc)"
  local input="$1"
  shift
  cat "${input}" | eval "${AOM_TEST_PREFIX}" "${encoder}" - \
    --test-decode=fatal \
    "$@" ${devnull}
}

# Wrapper function for running aomenc. Requires that LIBAOM_BIN_PATH points to
# the directory containing aomenc. $1 one is used as the input file path and
# shifted away. All remaining parameters are passed through to aomenc.
aomenc() {
  local encoder="$(aom_tool_path aomenc)"
  local input="$1"
  shift
  eval "${AOM_TEST_PREFIX}" "${encoder}" "${input}" \
    --test-decode=fatal \
    "$@" ${devnull}
}

aomenc_av1_ivf() {
  if [ "$(aomenc_can_encode_av1)" = "yes" ]; then
    local output="${AV1_IVF_FILE}"
    if [ -e "${AV1_IVF_FILE}" ]; then
      output="${AOM_TEST_OUTPUT_DIR}/av1_test.ivf"
    fi
    aomenc $(yuv_raw_input) \
      $(aomenc_encode_test_fast_params) \
      --ivf \
      --output="${output}" || return 1

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

aomenc_av1_ivf_rt() {
  if [ "$(aomenc_can_encode_av1)" = "yes" ]; then
    local output="${AV1_IVF_FILE}"
    if [ -e "${AV1_IVF_FILE}" ]; then
      output="${AOM_TEST_OUTPUT_DIR}/av1_test.ivf"
    fi
    aomenc $(yuv_raw_input) \
      $(aomenc_encode_test_rt_params) \
      --ivf \
      --output="${output}" || return 1

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

aomenc_av1_ivf_use_16bit_internal() {
  if [ "$(aomenc_can_encode_av1)" = "yes" ]; then
    local output="${AV1_IVF_FILE}"
    if [ -e "${AV1_IVF_FILE}" ]; then
      output="${AOM_TEST_OUTPUT_DIR}/av1_test_16bit.ivf"
    fi
    aomenc $(yuv_raw_input) \
      $(aomenc_encode_test_fast_params) \
      --ivf \
      --use-16bit-internal \
      --output="${output}" || return 1

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

aomenc_av1_obu_annexb() {
   if [ "$(aomenc_can_encode_av1)" = "yes" ]; then
    local output="${AV1_OBU_ANNEXB_FILE}"
    if [ -e "${AV1_OBU_ANNEXB_FILE}" ]; then
      output="${AOM_TEST_OUTPUT_DIR}/av1_test.annexb.obu"
    fi
    aomenc $(yuv_raw_input) \
      $(aomenc_encode_test_fast_params) \
      --obu \
      --annexb=1 \
      --output="${output}" || return 1

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

aomenc_av1_obu_section5() {
   if [ "$(aomenc_can_encode_av1)" = "yes" ]; then
    local output="${AV1_OBU_SEC5_FILE}"
    if [ -e "${AV1_OBU_SEC5_FILE}" ]; then
      output="${AOM_TEST_OUTPUT_DIR}/av1_test.section5.obu"
    fi
    aomenc $(yuv_raw_input) \
      $(aomenc_encode_test_fast_params) \
      --obu \
      --output="${output}" || return 1

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

aomenc_av1_webm() {
  if [ "$(aomenc_can_encode_av1)" = "yes" ] && \
     [ "$(webm_io_available)" = "yes" ]; then
    local output="${AV1_WEBM_FILE}"
    if [ -e "${AV1_WEBM_FILE}" ]; then
      output="${AOM_TEST_OUTPUT_DIR}/av1_test.webm"
    fi
    aomenc $(yuv_raw_input) \
      $(aomenc_encode_test_fast_params) \
      --output="${output}" || return 1

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

aomenc_av1_webm_1pass() {
  if [ "$(aomenc_can_encode_av1)" = "yes" ] && \
     [ "$(webm_io_available)" = "yes" ]; then
    local output="${AOM_TEST_OUTPUT_DIR}/av1_test.webm"
    aomenc $(yuv_raw_input) \
      $(aomenc_encode_test_fast_params) \
      --passes=1 \
      --output="${output}" || return 1

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

aomenc_av1_ivf_lossless() {
  if [ "$(aomenc_can_encode_av1)" = "yes" ]; then
    local output="${AOM_TEST_OUTPUT_DIR}/av1_lossless.ivf"
    aomenc $(yuv_raw_input) \
      $(aomenc_encode_test_fast_params) \
      --ivf \
      --output="${output}" \
      --lossless=1 || return 1

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

aomenc_av1_ivf_minq0_maxq0() {
  if [ "$(aomenc_can_encode_av1)" = "yes" ]; then
    local output="${AOM_TEST_OUTPUT_DIR}/av1_lossless_minq0_maxq0.ivf"
    aomenc $(yuv_raw_input) \
      $(aomenc_encode_test_fast_params) \
      --ivf \
      --output="${output}" \
      --min-q=0 \
      --max-q=0 || return 1

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

aomenc_av1_webm_lag5_frames10() {
  if [ "$(aomenc_can_encode_av1)" = "yes" ] && \
     [ "$(webm_io_available)" = "yes" ]; then
    local lag_total_frames=10
    local lag_frames=5
    local output="${AOM_TEST_OUTPUT_DIR}/av1_lag5_frames10.webm"
    aomenc $(yuv_raw_input) \
      $(aomenc_encode_test_fast_params) \
      --limit=${lag_total_frames} \
      --lag-in-frames=${lag_frames} \
      --output="${output}" || return 1

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

# TODO(fgalligan): Test that DisplayWidth is different than video width.
aomenc_av1_webm_non_square_par() {
  if [ "$(aomenc_can_encode_av1)" = "yes" ] && \
     [ "$(webm_io_available)" = "yes" ]; then
    local output="${AOM_TEST_OUTPUT_DIR}/av1_non_square_par.webm"
    aomenc $(y4m_input_non_square_par) \
      $(aomenc_encode_test_fast_params) \
      --output="${output}" || return 1

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

aomenc_av1_webm_cdf_update_mode() {
  if [ "$(aomenc_can_encode_av1)" = "yes" ] && \
     [ "$(webm_io_available)" = "yes" ]; then
    for mode in 0 1 2; do
      local output="${AOM_TEST_OUTPUT_DIR}/cdf_mode_${mode}.webm"
      aomenc $(yuv_raw_input) \
        $(aomenc_encode_test_fast_params) \
        --cdf-update-mode=${mode} \
        --output="${output}" || return 1

      if [ ! -e "${output}" ]; then
        elog "Output file does not exist."
        return 1
      fi
    done
  fi
}

if [ "$(realtime_only_build)" = "yes" ]; then
  aomenc_tests="aomenc_av1_ivf_rt"
else
  aomenc_tests="aomenc_av1_ivf
                aomenc_av1_ivf_rt
                aomenc_av1_obu_annexb
                aomenc_av1_obu_section5
                aomenc_av1_webm
                aomenc_av1_webm_1pass
                aomenc_av1_ivf_lossless
                aomenc_av1_ivf_minq0_maxq0
                aomenc_av1_ivf_use_16bit_internal
                aomenc_av1_webm_lag5_frames10
                aomenc_av1_webm_non_square_par
                aomenc_av1_webm_cdf_update_mode"
fi

run_tests aomenc_verify_environment "${aomenc_tests}"

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
## This file tests aomdec. To add new tests to this file, do the following:
##   1. Write a shell function (this is your test).
##   2. Add the function to aomdec_tests (on a new line).
##
. $(dirname $0)/tools_common.sh

AV1_MONOCHROME_B10="${LIBAOM_TEST_DATA_PATH}/av1-1-b10-24-monochrome.ivf"
AV1_MONOCHROME_B8="${LIBAOM_TEST_DATA_PATH}/av1-1-b8-24-monochrome.ivf"

# Environment check: Make sure input is available.
aomdec_verify_environment() {
  if [ "$(av1_encode_available)" != "yes" ] ; then
    if [ ! -e "${AV1_IVF_FILE}" ] || \
       [ ! -e "${AV1_OBU_ANNEXB_FILE}" ] || \
       [ ! -e "${AV1_OBU_SEC5_FILE}" ] || \
       [ ! -e "${AV1_WEBM_FILE}" ]; then
      elog "Libaom test data must exist before running this test script when " \
           " encoding is disabled. "
      return 1
    fi
  fi
  if [ ! -e "${AV1_MONOCHROME_B10}" ] || [ ! -e "${AV1_MONOCHROME_B8}" ]; then
    elog "Libaom test data must exist before running this test script."
  fi
  if [ -z "$(aom_tool_path aomdec)" ]; then
    elog "aomdec not found. It must exist in LIBAOM_BIN_PATH or its parent."
    return 1
  fi
}

# Wrapper function for running aomdec with pipe input. Requires that
# LIBAOM_BIN_PATH points to the directory containing aomdec. $1 is used as the
# input file path and shifted away. All remaining parameters are passed through
# to aomdec.
aomdec_pipe() {
  local input="$1"
  shift
  if [ ! -e "${input}" ]; then
    elog "Input file ($input) missing in aomdec_pipe()"
    return 1
  fi
  cat "${file}" | aomdec - "$@" ${devnull}
}


# Wrapper function for running aomdec. Requires that LIBAOM_BIN_PATH points to
# the directory containing aomdec. $1 one is used as the input file path and
# shifted away. All remaining parameters are passed through to aomdec.
aomdec() {
  local decoder="$(aom_tool_path aomdec)"
  local input="$1"
  shift
  eval "${AOM_TEST_PREFIX}" "${decoder}" "$input" "$@" ${devnull}
}

aomdec_can_decode_av1() {
  if [ "$(av1_decode_available)" = "yes" ]; then
    echo yes
  fi
}

aomdec_av1_ivf() {
  if [ "$(aomdec_can_decode_av1)" = "yes" ]; then
    local file="${AV1_IVF_FILE}"
    if [ ! -e "${file}" ]; then
      encode_yuv_raw_input_av1 "${file}" --ivf || return 1
    fi
    aomdec "${AV1_IVF_FILE}" --summary --noblit
  fi
}

aomdec_av1_ivf_error_resilient() {
  if [ "$(aomdec_can_decode_av1)" = "yes" ]; then
    local file="av1.error-resilient.ivf"
    if [ ! -e "${file}" ]; then
      encode_yuv_raw_input_av1 "${file}" --ivf --error-resilient=1 || return 1
    fi
    aomdec "${file}" --summary --noblit
  fi
}

ivf_multithread() {
  local row_mt="$1"
  if [ "$(aomdec_can_decode_av1)" = "yes" ]; then
    local file="${AV1_IVF_FILE}"
    if [ ! -e "${file}" ]; then
      encode_yuv_raw_input_av1 "${file}" --ivf || return 1
    fi
    for threads in 2 3 4 5 6 7 8; do
      aomdec "${file}" --summary --noblit --threads=$threads --row-mt=$row_mt \
        || return 1
    done
  fi
}

aomdec_av1_ivf_multithread() {
  ivf_multithread 0  # --row-mt=0
}

aomdec_av1_ivf_multithread_row_mt() {
  ivf_multithread 1  # --row-mt=1
}

aomdec_aom_ivf_pipe_input() {
  if [ "$(aomdec_can_decode_av1)" = "yes" ]; then
    local file="${AV1_IVF_FILE}"
    if [ ! -e "${file}" ]; then
      encode_yuv_raw_input_av1 "${file}" --ivf || return 1
    fi
    aomdec_pipe "${AV1_IVF_FILE}" --summary --noblit
  fi
}

aomdec_av1_obu_annexb() {
  if [ "$(aomdec_can_decode_av1)" = "yes" ]; then
    local file="${AV1_OBU_ANNEXB_FILE}"
    if [ ! -e "${file}" ]; then
      encode_yuv_raw_input_av1 "${file}" --obu --annexb=1 || return 1
    fi
    aomdec "${file}" --summary --noblit --annexb
  fi
}

aomdec_av1_obu_section5() {
  if [ "$(aomdec_can_decode_av1)" = "yes" ]; then
    local file="${AV1_OBU_SEC5_FILE}"
    if [ ! -e "${file}" ]; then
      encode_yuv_raw_input_av1 "${file}" --obu || return 1
    fi
    aomdec "${file}" --summary --noblit
  fi
}

aomdec_av1_webm() {
  if [ "$(aomdec_can_decode_av1)" = "yes" ] && \
     [ "$(webm_io_available)" = "yes" ]; then
    local file="${AV1_WEBM_FILE}"
    if [ ! -e "${file}" ]; then
      encode_yuv_raw_input_av1 "${file}" || return 1
    fi
    aomdec "${AV1_WEBM_FILE}" --summary --noblit
  fi
}

aomdec_av1_monochrome_yuv() {
  if [ "$(aomdec_can_decode_av1)" = "yes" ]; then
    local input="$1"
    local basename="$(basename "${input}")"
    local output="${basename}-%wx%h-%4.i420"
    local md5file="${AOM_TEST_OUTPUT_DIR}/${basename}.md5"
    local decoder="$(aom_tool_path aomdec)"
    # Note aomdec() is not used to avoid ${devnull} which may also redirect
    # stdout.
    eval "${AOM_TEST_PREFIX}" "${decoder}" --md5 --i420 \
      -o "${output}" "${input}" ">" "${md5file}" 2>&1 || return 1
    diff "${1}.md5" "${md5file}"
  fi
}

aomdec_av1_monochrome_yuv_8bit() {
  aomdec_av1_monochrome_yuv "${AV1_MONOCHROME_B8}"
}

aomdec_av1_monochrome_yuv_10bit() {
  aomdec_av1_monochrome_yuv "${AV1_MONOCHROME_B10}"
}

aomdec_tests="aomdec_av1_ivf
              aomdec_av1_ivf_multithread
              aomdec_av1_ivf_multithread_row_mt
              aomdec_aom_ivf_pipe_input
              aomdec_av1_monochrome_yuv_8bit"

if [ ! "$(realtime_only_build)" = "yes" ]; then
  aomdec_tests="${aomdec_tests}
                aomdec_av1_ivf_error_resilient
                aomdec_av1_obu_annexb
                aomdec_av1_obu_section5
                aomdec_av1_webm"
fi

if [ "$(highbitdepth_available)" = "yes" ]; then
  aomdec_tests="${aomdec_tests}
                aomdec_av1_monochrome_yuv_10bit"
fi

run_tests aomdec_verify_environment "${aomdec_tests}"

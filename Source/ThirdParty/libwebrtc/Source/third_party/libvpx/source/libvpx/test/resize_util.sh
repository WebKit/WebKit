#!/bin/sh
##
##  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##
##  This file tests the libvpx resize_util example code. To add new tests to
##  this file, do the following:
##    1. Write a shell function (this is your test).
##    2. Add the function to resize_util_tests (on a new line).
##
. $(dirname $0)/tools_common.sh

# Environment check: $YUV_RAW_INPUT is required.
resize_util_verify_environment() {
  if [ ! -e "${YUV_RAW_INPUT}" ]; then
    echo "Libvpx test data must exist in LIBVPX_TEST_DATA_PATH."
    return 1
  fi
}

# Resizes $YUV_RAW_INPUT using the resize_util example. $1 is the output
# dimensions that will be passed to resize_util.
resize_util() {
  local resizer="${LIBVPX_BIN_PATH}/resize_util${VPX_TEST_EXE_SUFFIX}"
  local output_file="${VPX_TEST_OUTPUT_DIR}/resize_util.raw"
  local frames_to_resize="10"
  local target_dimensions="$1"

  # resize_util is available only when CONFIG_SHARED is disabled.
  if [ -z "$(vpx_config_option_enabled CONFIG_SHARED)" ]; then
    if [ ! -x "${resizer}" ]; then
      elog "${resizer} does not exist or is not executable."
      return 1
    fi

    eval "${VPX_TEST_PREFIX}" "${resizer}" "${YUV_RAW_INPUT}" \
        "${YUV_RAW_INPUT_WIDTH}x${YUV_RAW_INPUT_HEIGHT}" \
        "${target_dimensions}" "${output_file}" ${frames_to_resize} \
        ${devnull}

    [ -e "${output_file}" ] || return 1
  fi
}

# Halves each dimension of $YUV_RAW_INPUT using resize_util().
resize_down() {
  local target_width=$((${YUV_RAW_INPUT_WIDTH} / 2))
  local target_height=$((${YUV_RAW_INPUT_HEIGHT} / 2))

  resize_util "${target_width}x${target_height}"
}

# Doubles each dimension of $YUV_RAW_INPUT using resize_util().
resize_up() {
  local target_width=$((${YUV_RAW_INPUT_WIDTH} * 2))
  local target_height=$((${YUV_RAW_INPUT_HEIGHT} * 2))

  resize_util "${target_width}x${target_height}"
}

resize_util_tests="resize_down
                   resize_up"

run_tests resize_util_verify_environment "${resize_util_tests}"

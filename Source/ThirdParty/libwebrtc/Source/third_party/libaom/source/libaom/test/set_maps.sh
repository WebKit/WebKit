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
## This file tests the libaom set_maps example. To add new tests to this file,
## do the following:
##   1. Write a shell function (this is your test).
##   2. Add the function to set_maps_tests (on a new line).
##
. $(dirname $0)/tools_common.sh

# Environment check: $YUV_RAW_INPUT is required, and set_maps must exist in
# $LIBAOM_BIN_PATH.
set_maps_verify_environment() {
  if [ ! -e "${YUV_RAW_INPUT}" ]; then
    echo "Libaom test data must exist in LIBAOM_TEST_DATA_PATH."
    return 1
  fi
  if [ -z "$(aom_tool_path set_maps)" ]; then
    elog "set_maps not found. It must exist in LIBAOM_BIN_PATH or its parent."
    return 1
  fi
}

# Runs set_maps using the codec specified by $1.
set_maps() {
  local encoder="$(aom_tool_path set_maps)"
  local codec="$1"
  local output_file="${AOM_TEST_OUTPUT_DIR}/set_maps_${codec}.ivf"

  eval "${AOM_TEST_PREFIX}" "${encoder}" "${codec}" "${YUV_RAW_INPUT_WIDTH}" \
      "${YUV_RAW_INPUT_HEIGHT}" "${YUV_RAW_INPUT}" "${output_file}" \
      ${devnull} || return 1

  [ -e "${output_file}" ] || return 1
}

set_maps_av1() {
  if [ "$(av1_encode_available)" = "yes" ]; then
    set_maps av1 || return 1
  fi
}

set_maps_tests="set_maps_av1"

run_tests set_maps_verify_environment "${set_maps_tests}"

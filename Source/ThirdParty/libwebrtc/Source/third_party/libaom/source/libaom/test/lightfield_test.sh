#!/bin/sh
## Copyright (c) 2018, Alliance for Open Media. All rights reserved.
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
## This file tests the lightfield example.
##
. $(dirname $0)/tools_common.sh

# Environment check: $infile is required.
lightfield_test_verify_environment() {
  local infile="${LIBAOM_TEST_DATA_PATH}/vase10x10.yuv"
  if [ ! -e "${infile}" ]; then
    echo "Libaom test data must exist in LIBAOM_TEST_DATA_PATH."
    return 1
  fi
}

# Run the lightfield example
lightfield_test() {
  local img_width=1024
  local img_height=1024
  local lf_width=10
  local lf_height=10
  local lf_blocksize=5
  local num_references=4
  local num_tile_lists=2

  # Encode the lightfield.
  local encoder="${LIBAOM_BIN_PATH}/lightfield_encoder${AOM_TEST_EXE_SUFFIX}"
  local yuv_file="${LIBAOM_TEST_DATA_PATH}/vase10x10.yuv"
  local lf_file="${AOM_TEST_OUTPUT_DIR}/vase10x10.ivf"
  if [ ! -x "${encoder}" ]; then
    elog "${encoder} does not exist or is not executable."
    return 1
  fi

  eval "${AOM_TEST_PREFIX}" "${encoder}" "${img_width}" "${img_height}" \
      "${yuv_file}" "${lf_file}" "${lf_width}" \
      "${lf_height}" "${lf_blocksize}" ${devnull} || return 1

  [ -e "${lf_file}" ] || return 1

  # Check to ensure all camera frames have the identical frame header. If not identical, this test fails.
  for i in ./fh*; do
    diff ./fh004 $i > /dev/null
      if [ $? -eq 1 ]; then
      return 1
    fi
  done

  # Check to ensure all camera frames use the identical frame context. If not identical, this test fails.
  for i in ./fc*; do
    diff ./fc004 $i > /dev/null
      if [ $? -eq 1 ]; then
      return 1
    fi
  done

  # Parse lightfield bitstream to construct and output a new bitstream that can
  # be decoded by an AV1 decoder.
  local bs_decoder="${LIBAOM_BIN_PATH}/lightfield_bitstream_parsing${AOM_TEST_EXE_SUFFIX}"
  local tl_file="${AOM_TEST_OUTPUT_DIR}/vase_tile_list.ivf"
  local tl_text_file="${LIBAOM_TEST_DATA_PATH}/vase10x10_tiles.txt"
  if [ ! -x "${bs_decoder}" ]; then
    elog "${bs_decoder} does not exist or is not executable."
    return 1
  fi

  eval "${AOM_TEST_PREFIX}" "${bs_decoder}" "${lf_file}" "${tl_file}" \
      "${num_references}" "${tl_text_file}" ${devnull} || return 1

  [ -e "${tl_file}" ] || return 1

  # Run lightfield tile list decoder
  local tl_decoder="${LIBAOM_BIN_PATH}/lightfield_tile_list_decoder${AOM_TEST_EXE_SUFFIX}"
  local tl_outfile="${AOM_TEST_OUTPUT_DIR}/vase_tile_list.yuv"
  if [ ! -x "${tl_decoder}" ]; then
    elog "${tl_decoder} does not exist or is not executable."
    return 1
  fi

  eval "${AOM_TEST_PREFIX}" "${tl_decoder}" "${tl_file}" "${tl_outfile}" \
      "${num_references}" "${num_tile_lists}" ${devnull} || return 1

  [ -e "${tl_outfile}" ] || return 1

  # Run reference lightfield decoder
  local ref_decoder="${LIBAOM_BIN_PATH}/lightfield_decoder${AOM_TEST_EXE_SUFFIX}"
  local tl_reffile="${AOM_TEST_OUTPUT_DIR}/vase_reference.yuv"
  if [ ! -x "${ref_decoder}" ]; then
    elog "${ref_decoder} does not exist or is not executable."
    return 1
  fi

  eval "${AOM_TEST_PREFIX}" "${ref_decoder}" "${lf_file}" "${tl_reffile}" \
      "${num_references}" "${tl_text_file}" ${devnull} || return 1

  [ -e "${tl_reffile}" ] || return 1

  # Check if tl_outfile and tl_reffile are identical. If not identical, this test fails.
  diff ${tl_outfile} ${tl_reffile} > /dev/null
  if [ $? -eq 1 ]; then
    return 1
  fi
}

lightfield_test_tests="lightfield_test"

run_tests lightfield_test_verify_environment "${lightfield_test_tests}"

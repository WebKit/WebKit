#!/bin/bash
# Copyright 2023 Google LLC
# SPDX-License-Identifier: BSD-2-Clause
#
# Test that compares the structure (boxes) of AVIF files metadata, dumped as xml,
# with golden files stored in tests/data/goldens/.
# Depends on the command line tool MP4Box.
#
# Usage:
# test_cmd_enc_boxes_golden.sh <avifenc_dir> <mp4box_dir> <testdata_dir>
#                              <test_output_dir>
# But most of the time you will be running this test with 'make test' or
# 'ctest -V -R test_cmd_enc_boxes_golden'

# ===================================
# ========== Encode files ===========
# ===================================
# To add new test case, just add new encode commands to this function.
# All .avif files created here will become test cases.
encode_test_files() {
    AVIFENC="$1"
    TESTDATA_DIR="$2"

    # Still images.
    for f in "kodim03_yuv420_8bpc.y4m" "circle-trns-after-plte.png" "paris_icc_exif_xmp.png"; do
        "${AVIFENC}" -s 9 "${TESTDATA_DIR}/$f" -o "$f.avif"
    done
    
    # Grid.
    "${AVIFENC}" -s 9 --grid 2x2 "${TESTDATA_DIR}/dog_exif_extended_xmp_icc.jpg" \
      -o "dog_exif_extended_xmp_icc.jpg.avif"

    # Animation.
    "${AVIFENC}" -s 9 "${TESTDATA_DIR}/kodim03_yuv420_8bpc.y4m" \
      "${TESTDATA_DIR}/kodim23_yuv420_8bpc.y4m" -o "kodim03_23_animation.avif"

    # Animation with only keyframes.
    "${AVIFENC}" -s 9 --keyframe 1 "${TESTDATA_DIR}/kodim03_yuv420_8bpc.y4m" \
      "${TESTDATA_DIR}/kodim23_yuv420_8bpc.y4m" -o "kodim03_23_animation_keyframes.avif"
}

export -f encode_test_files
source $(dirname "$0")/golden_test_common.sh

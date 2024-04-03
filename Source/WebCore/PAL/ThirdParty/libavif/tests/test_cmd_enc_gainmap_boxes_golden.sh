#!/bin/bash
# Copyright 2023 Google LLC
# SPDX-License-Identifier: BSD-2-Clause
#
# Same as test_cmd_enc_boxes_golden.sh but for when gain maps are enabled.
# Compares the structure (boxes) of AVIF files metadata, dumped as xml,
# with golden files stored in tests/data/goldens/.
# Depends on the command line tool MP4Box.
#
# Usage:
# test_cmd_enc_gainmap_boxes_golden.sh <avifenc_dir> <mp4box_dir> <testdata_dir>
#                                      <test_output_dir>
# But most of the time you will be running this test with 'make test' or
# 'ctest -V -R test_cmd_enc_gainmap_boxes_golden'

# ===================================
# ========== Encode files ===========
# ===================================
# To add new test case, just add new encode commands to this function.
# All .avif files created here will become test cases.
encode_test_files() {
    # Image with a gain map.
    for f in "paris_exif_xmp_gainmap_bigendian.jpg" \
        "paris_exif_xmp_icc_gainmap_bigendian.jpg" \
        "paris_exif_xmp_gainmap_littleendian.jpg"; do
        "${AVIFENC}" -s 9 "${TESTDATA_DIR}/$f" -o "$f.avif"
    done

    # Ignore gain map.
    "${AVIFENC}" -s 9 --ignore-gain-map "${TESTDATA_DIR}/paris_exif_xmp_gainmap_bigendian.jpg" \
      -o "paris_exif_xmp_gainmap_bigendian_ignore.jpg.avif"
}

export -f encode_test_files
source $(dirname "$0")/golden_test_common.sh

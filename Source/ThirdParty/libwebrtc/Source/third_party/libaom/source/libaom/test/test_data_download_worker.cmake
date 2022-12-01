#
# Copyright (c) 2017, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.
#
include("${AOM_ROOT}/test/test_data_util.cmake")

# https://github.com/cheshirekow/cmake_format/issues/34
# cmake-format: off
if (NOT AOM_ROOT OR NOT AOM_CONFIG_DIR OR NOT AOM_TEST_FILE
    OR NOT AOM_TEST_CHECKSUM)
  message(FATAL_ERROR
          "AOM_ROOT, AOM_CONFIG_DIR, AOM_TEST_FILE and AOM_TEST_CHECKSUM must be
          defined.")
endif ()
# cmake-format: on

set(AOM_TEST_DATA_URL "https://storage.googleapis.com/aom-test-data")

if(NOT AOM_TEST_DATA_PATH)
  set(AOM_TEST_DATA_PATH "$ENV{LIBAOM_TEST_DATA_PATH}")
endif()

if("${AOM_TEST_DATA_PATH}" STREQUAL "")
  message(
    WARNING "Writing test data to ${AOM_CONFIG_DIR}, set "
            "$LIBAOM_TEST_DATA_PATH in your environment to avoid this warning.")
  set(AOM_TEST_DATA_PATH "${AOM_CONFIG_DIR}")
endif()

if(NOT EXISTS "${AOM_TEST_DATA_PATH}")
  file(MAKE_DIRECTORY "${AOM_TEST_DATA_PATH}")
endif()

expand_test_file_paths("AOM_TEST_FILE" "${AOM_TEST_DATA_PATH}" "filepath")
expand_test_file_paths("AOM_TEST_FILE" "${AOM_TEST_DATA_URL}" "url")

check_file("${filepath}" "${AOM_TEST_CHECKSUM}" "needs_download")
if(needs_download)
  download_test_file("${url}" "${AOM_TEST_CHECKSUM}" "${filepath}")
endif()

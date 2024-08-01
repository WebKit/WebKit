#
# Copyright (c) 2017, Alliance for Open Media. All rights reserved.
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.
#
if(NOT GTEST_TOTAL_SHARDS
   OR "${GTEST_SHARD_INDEX}" STREQUAL ""
   OR NOT TEST_LIBAOM)
  message(
    FATAL_ERROR
      "The variables GTEST_SHARD_INDEX, GTEST_TOTAL_SHARDS and TEST_LIBAOM
          must be defined.")
endif()

set($ENV{GTEST_SHARD_INDEX} ${GTEST_SHARD_INDEX})
set($ENV{GTEST_TOTAL_SHARDS} ${GTEST_TOTAL_SHARDS})
execute_process(COMMAND ${TEST_LIBAOM} RESULT_VARIABLE test_result)
set(test_message "Test shard ${GTEST_SHARD_INDEX}/${GTEST_TOTAL_SHARDS} result")
message("${test_message}: ${test_result}")

if(NOT "${test_result}" STREQUAL "0")
  message(FATAL_ERROR "${test_message}: FAILED, non-zero exit code.")
endif()

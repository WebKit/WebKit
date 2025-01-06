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
## This file runs all of the tests for the libaom examples.
##
readonly EXEC_DIR="$(pwd)"
. $(dirname $0)/tools_common.sh

example_tests=$(ls -r $(dirname $0)/*.sh)

# List of script names to exclude.
exclude_list="best_encode examples run_encodes tools_common"

if [ "$(realtime_only_build)" = "yes" ]; then
  exclude_list="${exclude_list} twopass_encoder simple_decoder lightfield_test"
fi

# Filter out the scripts in $exclude_list.
for word in ${exclude_list}; do
  example_tests=$(filter_strings "${example_tests}" "${word}" exclude)
done

for test in ${example_tests}; do
  # Source each test script so that exporting variables can be avoided.
  AOM_TEST_NAME="$(basename ${test%.*})"
  . "${test}"
  # Restore the working directory to the one at the beginning of execution.
  # This avoids side-effects from tests that change the directory.
  cd "${EXEC_DIR}"
done

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
##  This file contains shell code shared by test scripts for libaom tools.

# Use $AOM_TEST_TOOLS_COMMON_SH as a pseudo include guard.
if [ -z "${AOM_TEST_TOOLS_COMMON_SH}" ]; then
AOM_TEST_TOOLS_COMMON_SH=included

set -e
devnull='> /dev/null 2>&1'
AOM_TEST_PREFIX=""

elog() {
  echo "$@" 1>&2
}

vlog() {
  if [ "${AOM_TEST_VERBOSE_OUTPUT}" = "yes" ]; then
    echo "$@"
  fi
}

# Sets $AOM_TOOL_TEST to the name specified by positional parameter one.
test_begin() {
  AOM_TOOL_TEST="${1}"
}

# Clears the AOM_TOOL_TEST variable after confirming that $AOM_TOOL_TEST matches
# positional parameter one.
test_end() {
  if [ "$1" != "${AOM_TOOL_TEST}" ]; then
    echo "FAIL completed test mismatch!."
    echo "  completed test: ${1}"
    echo "  active test: ${AOM_TOOL_TEST}."
    return 1
  fi
  AOM_TOOL_TEST='<unset>'
}

# Echoes the target configuration being tested.
test_configuration_target() {
  aom_config_c="${LIBAOM_CONFIG_PATH}/config/aom_config.c"
  # Clean up the cfg pointer line from aom_config.c for easier re-use by
  # someone examining a failure in the example tests.
  # 1. Run grep on aom_config.c for cfg and limit the results to 1.
  # 2. Split the line using ' = ' as separator.
  # 3. Abuse sed to consume the leading " and trailing "; from the assignment
  #    to the cfg pointer.
  cmake_config=$(awk -F ' = ' '/cfg/ { print $NF; exit }' "${aom_config_c}" \
    | sed -e s/\"// -e s/\"\;//)
  echo cmake generated via command: cmake path/to/aom ${cmake_config}
}

# Trap function used for failure reports and tool output directory removal.
# When the contents of $AOM_TOOL_TEST do not match the string '<unset>', reports
# failure of test stored in $AOM_TOOL_TEST.
cleanup() {
  if [ -n "${AOM_TOOL_TEST}" ] && [ "${AOM_TOOL_TEST}" != '<unset>' ]; then
    echo "FAIL: $AOM_TOOL_TEST"
  fi
  if [ "${AOM_TEST_PRESERVE_OUTPUT}" = "yes" ]; then
    return
  fi
  if [ -n "${AOM_TEST_OUTPUT_DIR}" ] && [ -d "${AOM_TEST_OUTPUT_DIR}" ]; then
    rm -rf "${AOM_TEST_OUTPUT_DIR}"
  fi
}

# Echoes the version string assigned to the VERSION_STRING_NOSP variable defined
# in $LIBAOM_CONFIG_PATH/config/aom_version.h to stdout.
cmake_version() {
  aom_version_h="${LIBAOM_CONFIG_PATH}/config/aom_version.h"

  # Find VERSION_STRING_NOSP line, split it with '"' and print the next to last
  # field to output the version string to stdout.
  aom_version=$(awk -F \" '/VERSION_STRING_NOSP/ {print $(NF-1)}' \
    "${aom_version_h}")
  echo "v${aom_version}"
}

# Echoes current git version as reported by running 'git describe', or the
# version used by the cmake build when git is unavailable.
source_version() {
  if git --version > /dev/null 2>&1; then
    (cd "$(dirname "${0}")"
    git describe)
  else
    cmake_version
  fi
}

# Echoes warnings to stdout when source version and CMake build generated
# version are out of sync.
check_version_strings() {
  cmake_version=$(cmake_version)
  source_version=$(source_version)

  if [ "${cmake_version}" != "${source_version}" ]; then
    echo "Warning: version has changed since last cmake run."
    vlog "  cmake version: ${cmake_version} version now: ${source_version}"
  fi
}

# $1 is the name of an environment variable containing a directory name to
# test.
test_env_var_dir() {
  local dir=$(eval echo "\${$1}")
  if [ ! -d "${dir}" ]; then
    elog "'${dir}': No such directory"
    elog "The $1 environment variable must be set to a valid directory."
    return 1
  fi
}

# This script requires that the LIBAOM_BIN_PATH, LIBAOM_CONFIG_PATH, and
# LIBAOM_TEST_DATA_PATH variables are in the environment: Confirm that
# the variables are set and that they all evaluate to directory paths.
verify_aom_test_environment() {
  test_env_var_dir "LIBAOM_BIN_PATH" \
    && test_env_var_dir "LIBAOM_CONFIG_PATH" \
    && test_env_var_dir "LIBAOM_TEST_DATA_PATH"
}

# Greps aom_config.h in LIBAOM_CONFIG_PATH for positional parameter one, which
# should be a LIBAOM preprocessor flag. Echoes yes to stdout when the feature
# is available.
aom_config_option_enabled() {
  aom_config_option="${1}"
  aom_config_file="${LIBAOM_CONFIG_PATH}/config/aom_config.h"
  config_line=$(grep "${aom_config_option}" "${aom_config_file}")
  if echo "${config_line}" | egrep -q '1$'; then
    echo yes
  fi
}

# Echoes yes when output of test_configuration_target() contains win32 or win64.
is_windows_target() {
  if test_configuration_target \
     | grep -q -e win32 -e win64 > /dev/null 2>&1; then
    echo yes
  fi
}

# Echoes path to $1 when it's executable and exists in one of the directories
# included in $tool_paths, or an empty string. Caller is responsible for testing
# the string once the function returns.
aom_tool_path() {
  local tool_name="$1"
  local root_path="${LIBAOM_BIN_PATH}"
  local suffix="${AOM_TEST_EXE_SUFFIX}"
  local tool_paths="\
    ${root_path}/${tool_name}${suffix} \
    ${root_path}/../${tool_name}${suffix} \
    ${root_path}/tools/${tool_name}${suffix} \
    ${root_path}/../tools/${tool_name}${suffix}"

  local toolpath=""

  for tool_path in ${tool_paths}; do
    if [ -x "${tool_path}" ] && [ -f "${tool_path}" ]; then
      echo "${tool_path}"
      return 0
    fi
  done

  return 1
}

# Echoes yes to stdout when the file named by positional parameter one exists
# in LIBAOM_BIN_PATH, and is executable.
aom_tool_available() {
  local tool_name="$1"
  local tool="${LIBAOM_BIN_PATH}/${tool_name}${AOM_TEST_EXE_SUFFIX}"
  [ -x "${tool}" ] && echo yes
}

# Echoes yes to stdout when aom_config_option_enabled() reports yes for
# CONFIG_AV1_DECODER.
av1_decode_available() {
  [ "$(aom_config_option_enabled CONFIG_AV1_DECODER)" = "yes" ] && echo yes
}

# Echoes yes to stdout when aom_config_option_enabled() reports yes for
# CONFIG_AV1_ENCODER.
av1_encode_available() {
  [ "$(aom_config_option_enabled CONFIG_AV1_ENCODER)" = "yes" ] && echo yes
}

# Echoes "fast" encode params for use with aomenc.
aomenc_encode_test_fast_params() {
  echo "--cpu-used=2
        --limit=${AV1_ENCODE_TEST_FRAME_LIMIT}
        --lag-in-frames=0
        --test-decode=fatal"
}

# Echoes realtime encode params for use with aomenc.
aomenc_encode_test_rt_params() {
  echo "--limit=${AV1_ENCODE_TEST_FRAME_LIMIT}
        --test-decode=fatal
        --enable-tpl-model=0
        --deltaq-mode=0
        --enable-order-hint=0
        --profile=0
        --static-thresh=0
        --end-usage=cbr
        --cpu-used=7
        --passes=1
        --usage=1
        --lag-in-frames=0
        --aq-mode=3
        --enable-obmc=0
        --enable-warped-motion=0
        --enable-ref-frame-mvs=0
        --enable-cdef=1
        --enable-order-hint=0
        --coeff-cost-upd-freq=3
        --mode-cost-upd-freq=3
        --mv-cost-upd-freq=3"
}

# Echoes yes to stdout when aom_config_option_enabled() reports yes for
# CONFIG_AV1_HIGHBITDEPTH.
highbitdepth_available() {
  [ "$(aom_config_option_enabled CONFIG_AV1_HIGHBITDEPTH)" = "yes" ] && echo yes
}

# Echoes yes to stdout when aom_config_option_enabled() reports yes for
# CONFIG_WEBM_IO.
webm_io_available() {
  [ "$(aom_config_option_enabled CONFIG_WEBM_IO)" = "yes" ] && echo yes
}

# Echoes yes to stdout when aom_config_option_enabled() reports yes for
# CONFIG_REALTIME_ONLY.
realtime_only_build() {
  [ "$(aom_config_option_enabled CONFIG_REALTIME_ONLY)" = "yes" ] && echo yes
}

# Filters strings from $1 using the filter specified by $2. Filter behavior
# depends on the presence of $3. When $3 is present, strings that match the
# filter are excluded. When $3 is omitted, strings matching the filter are
# included.
# The filtered result is echoed to stdout.
filter_strings() {
  strings=${1}
  filter=${2}
  exclude=${3}

  if [ -n "${exclude}" ]; then
    # When positional parameter three exists the caller wants to remove strings.
    # Tell grep to invert matches using the -v argument.
    exclude='-v'
  else
    unset exclude
  fi

  if [ -n "${filter}" ]; then
    for s in ${strings}; do
      if echo "${s}" | egrep -q ${exclude} "${filter}" > /dev/null 2>&1; then
        filtered_strings="${filtered_strings} ${s}"
      fi
    done
  else
    filtered_strings="${strings}"
  fi
  echo "${filtered_strings}"
}

# Runs user test functions passed via positional parameters one and two.
# Functions in positional parameter one are treated as environment verification
# functions and are run unconditionally. Functions in positional parameter two
# are run according to the rules specified in aom_test_usage().
run_tests() {
  local env_tests="verify_aom_test_environment $1"
  local tests_to_filter="$2"
  local test_name="${AOM_TEST_NAME}"

  if [ -z "${test_name}" ]; then
    test_name="$(basename "${0%.*}")"
  fi

  if [ "${AOM_TEST_RUN_DISABLED_TESTS}" != "yes" ]; then
    # Filter out DISABLED tests.
    tests_to_filter=$(filter_strings "${tests_to_filter}" ^DISABLED exclude)
  fi

  if [ -n "${AOM_TEST_FILTER}" ]; then
    # Remove tests not matching the user's filter.
    tests_to_filter=$(filter_strings "${tests_to_filter}" ${AOM_TEST_FILTER})
  fi

  # User requested test listing: Dump test names and return.
  if [ "${AOM_TEST_LIST_TESTS}" = "yes" ]; then
    for test_name in $tests_to_filter; do
      echo ${test_name}
    done
    return
  fi

  # Don't bother with the environment tests if everything else was disabled.
  [ -z "${tests_to_filter}" ] && return

  # Combine environment and actual tests.
  local tests_to_run="${env_tests} ${tests_to_filter}"

  # av1_c_vs_simd_encode is a standalone test, and it doesn't need to check the
  # version string.
  if [ "${test_name}" != "av1_c_vs_simd_encode" ]; then
    check_version_strings
  fi

  # Run tests.
  for test in ${tests_to_run}; do
    test_begin "${test}"
    vlog "  RUN  ${test}"
    "${test}"
    vlog "  PASS ${test}"
    test_end "${test}"
  done

  local tested_config="$(test_configuration_target) @ $(source_version)"
  echo "${test_name}: Done, all tests pass for ${tested_config}."
}

aom_test_usage() {
cat << EOF
  Usage: ${0##*/} [arguments]
    --bin-path <path to libaom binaries directory>
    --config-path <path to libaom config directory>
    --filter <filter>: User test filter. Only tests matching filter are run.
    --run-disabled-tests: Run disabled tests.
    --help: Display this message and exit.
    --test-data-path <path to libaom test data directory>
    --show-program-output: Shows output from all programs being tested.
    --prefix: Allows for a user specified prefix to be inserted before all test
              programs. Grants the ability, for example, to run test programs
              within valgrind.
    --list-tests: List all test names and exit without actually running tests.
    --verbose: Verbose output.

    When the --bin-path option is not specified the script attempts to use
    \$LIBAOM_BIN_PATH and then the current directory.

    When the --config-path option is not specified the script attempts to use
    \$LIBAOM_CONFIG_PATH and then the current directory.

    When the -test-data-path option is not specified the script attempts to use
    \$LIBAOM_TEST_DATA_PATH and then the current directory.
EOF
}

# Returns non-zero (failure) when required environment variables are empty
# strings.
aom_test_check_environment() {
  if [ -z "${LIBAOM_BIN_PATH}" ] || \
     [ -z "${LIBAOM_CONFIG_PATH}" ] || \
     [ -z "${LIBAOM_TEST_DATA_PATH}" ]; then
    return 1
  fi
}

# Echo aomenc command line parameters allowing use of a raw yuv file as
# input to aomenc.
yuv_raw_input() {
  echo ""${YUV_RAW_INPUT}"
       --width="${YUV_RAW_INPUT_WIDTH}"
       --height="${YUV_RAW_INPUT_HEIGHT}""
}

# Do a small encode for testing decoders.
encode_yuv_raw_input_av1() {
  if [ "$(av1_encode_available)" = "yes" ]; then
    local output="$1"
    local encoder="$(aom_tool_path aomenc)"
    shift
    eval "${encoder}" $(yuv_raw_input) \
      $(aomenc_encode_test_fast_params) \
      --output="${output}" \
      $@ \
      ${devnull}

    if [ ! -e "${output}" ]; then
      elog "Output file does not exist."
      return 1
    fi
  fi
}

# Parse the command line.
while [ -n "$1" ]; do
  case "$1" in
    --bin-path)
      LIBAOM_BIN_PATH="$2"
      shift
      ;;
    --config-path)
      LIBAOM_CONFIG_PATH="$2"
      shift
      ;;
    --filter)
      AOM_TEST_FILTER="$2"
      shift
      ;;
    --run-disabled-tests)
      AOM_TEST_RUN_DISABLED_TESTS=yes
      ;;
    --help)
      aom_test_usage
      exit
      ;;
    --test-data-path)
      LIBAOM_TEST_DATA_PATH="$2"
      shift
      ;;
    --prefix)
      AOM_TEST_PREFIX="$2"
      shift
      ;;
    --verbose)
      AOM_TEST_VERBOSE_OUTPUT=yes
      ;;
    --show-program-output)
      devnull=
      ;;
    --list-tests)
      AOM_TEST_LIST_TESTS=yes
      ;;
    *)
      aom_test_usage
      exit 1
      ;;
  esac
  shift
done

# Handle running the tests from a build directory without arguments when running
# the tests on *nix/macosx.
LIBAOM_BIN_PATH="${LIBAOM_BIN_PATH:-.}"
LIBAOM_CONFIG_PATH="${LIBAOM_CONFIG_PATH:-.}"
LIBAOM_TEST_DATA_PATH="${LIBAOM_TEST_DATA_PATH:-.}"

# Create a temporary directory for output files, and a trap to clean it up.
if [ -n "${TMPDIR}" ]; then
  AOM_TEST_TEMP_ROOT="${TMPDIR}"
elif [ -n "${TEMPDIR}" ]; then
  AOM_TEST_TEMP_ROOT="${TEMPDIR}"
else
  AOM_TEST_TEMP_ROOT=/tmp
fi

AOM_TEST_OUTPUT_DIR="${AOM_TEST_OUTPUT_DIR:-${AOM_TEST_TEMP_ROOT}/aom_test_$$}"

if ! mkdir -p "${AOM_TEST_OUTPUT_DIR}" || \
   [ ! -d "${AOM_TEST_OUTPUT_DIR}" ]; then
  echo "${0##*/}: Cannot create output directory, giving up."
  echo "${0##*/}:   AOM_TEST_OUTPUT_DIR=${AOM_TEST_OUTPUT_DIR}"
  exit 1
fi

AOM_TEST_PRESERVE_OUTPUT=${AOM_TEST_PRESERVE_OUTPUT:-no}

# This checking requires config/aom_config.c that is available in Jenkins
# testing.
if [ "$(is_windows_target)" = "yes" ]; then
  AOM_TEST_EXE_SUFFIX=".exe"
fi

# Variables shared by tests.
AV1_ENCODE_CPU_USED=${AV1_ENCODE_CPU_USED:-1}
AV1_ENCODE_TEST_FRAME_LIMIT=${AV1_ENCODE_TEST_FRAME_LIMIT:-5}
AV1_IVF_FILE="${AV1_IVF_FILE:-${AOM_TEST_OUTPUT_DIR}/av1.ivf}"
AV1_OBU_ANNEXB_FILE="${AV1_OBU_ANNEXB_FILE:-${AOM_TEST_OUTPUT_DIR}/av1.annexb.obu}"
AV1_OBU_SEC5_FILE="${AV1_OBU_SEC5_FILE:-${AOM_TEST_OUTPUT_DIR}/av1.section5.obu}"
AV1_WEBM_FILE="${AV1_WEBM_FILE:-${AOM_TEST_OUTPUT_DIR}/av1.webm}"

YUV_RAW_INPUT="${LIBAOM_TEST_DATA_PATH}/hantro_collage_w352h288.yuv"
YUV_RAW_INPUT_WIDTH=352
YUV_RAW_INPUT_HEIGHT=288

Y4M_NOSQ_PAR_INPUT="${LIBAOM_TEST_DATA_PATH}/park_joy_90p_8_420_a10-1.y4m"
Y4M_720P_INPUT="${LIBAOM_TEST_DATA_PATH}/niklas_1280_720_30.y4m"

# Setup a trap function to clean up after tests complete.
trap cleanup EXIT

vlog "$(basename "${0%.*}") test configuration:
  LIBAOM_BIN_PATH=${LIBAOM_BIN_PATH}
  LIBAOM_CONFIG_PATH=${LIBAOM_CONFIG_PATH}
  LIBAOM_TEST_DATA_PATH=${LIBAOM_TEST_DATA_PATH}
  AOM_TEST_EXE_SUFFIX=${AOM_TEST_EXE_SUFFIX}
  AOM_TEST_FILTER=${AOM_TEST_FILTER}
  AOM_TEST_LIST_TESTS=${AOM_TEST_LIST_TESTS}
  AOM_TEST_OUTPUT_DIR=${AOM_TEST_OUTPUT_DIR}
  AOM_TEST_PREFIX=${AOM_TEST_PREFIX}
  AOM_TEST_PRESERVE_OUTPUT=${AOM_TEST_PRESERVE_OUTPUT}
  AOM_TEST_RUN_DISABLED_TESTS=${AOM_TEST_RUN_DISABLED_TESTS}
  AOM_TEST_SHOW_PROGRAM_OUTPUT=${AOM_TEST_SHOW_PROGRAM_OUTPUT}
  AOM_TEST_TEMP_ROOT=${AOM_TEST_TEMP_ROOT}
  AOM_TEST_VERBOSE_OUTPUT=${AOM_TEST_VERBOSE_OUTPUT}
  AV1_ENCODE_CPU_USED=${AV1_ENCODE_CPU_USED}
  AV1_ENCODE_TEST_FRAME_LIMIT=${AV1_ENCODE_TEST_FRAME_LIMIT}
  AV1_IVF_FILE=${AV1_IVF_FILE}
  AV1_OBU_ANNEXB_FILE=${AV1_OBU_ANNEXB_FILE}
  AV1_OBU_SEC5_FILE=${AV1_OBU_SEC5_FILE}
  AV1_WEBM_FILE=${AV1_WEBM_FILE}
  YUV_RAW_INPUT=${YUV_RAW_INPUT}
  YUV_RAW_INPUT_WIDTH=${YUV_RAW_INPUT_WIDTH}
  YUV_RAW_INPUT_HEIGHT=${YUV_RAW_INPUT_HEIGHT}
  Y4M_NOSQ_PAR_INPUT=${Y4M_NOSQ_PAR_INPUT}"

fi  # End $AOM_TEST_TOOLS_COMMON_SH pseudo include guard.

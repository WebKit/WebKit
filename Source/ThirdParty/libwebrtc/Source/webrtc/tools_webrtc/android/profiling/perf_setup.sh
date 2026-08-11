#!/bin/bash

# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
#
# Usage:
#
#   It is assumed that a release build of AppRTCMobile exists and has been
#   installed on an Android device which supports USB debugging.
#
#   Source this script once from the WebRTC src/ directory and resolve any
#   reported issues. Add relative path to build directory as parameter.
#   Required tools will be downloaded if they don't already exist.
#
#   Once all tests are passed, a list of available functions will be given.
#   Use these functions to do the actual profiling and visualization of the
#   results.
#
#   Note that, using a rooted device is recommended since it allows us to
#   resolve kernel symbols (kallsyms) as well.
#
# Example usage:
#
#   > . tools_webrtc/android/profiling/perf_setup.sh out/Release
#   > perf_record 120
#   > flame_graph
#   > plot_flame_graph
#   > perf_cleanup

if [ -n "$ZSH_VERSION" ]; then
  # Running inside zsh.
  SCRIPT_PATH="${(%):-%N}"
else
  # Running inside something else (most likely bash).
  SCRIPT_PATH="${BASH_SOURCE[0]}"
fi
SCRIPT_DIR="$(cd $(dirname "$SCRIPT_PATH") && pwd -P)"
source "${SCRIPT_DIR}/utilities.sh"

# Root directory for local symbol cache.
SYMBOL_DIR="${TMPDIR:-/tmp}/android_symbols"
# Used as a temporary folder on the Android device for data storage.
DEV_TMP_DIR="/data/local/tmp"
# Relative path to native shared library containing symbols.
NATIVE_LIB_PATH="/lib.unstripped/libjingle_peerconnection_so.so"
# Name of application package for the AppRTCMobile demo.
APP_NAME="org.appspot.apprtc"

# Make sure we're being sourced.
if [[ -n "${BASH_VERSION}" && "${BASH_SOURCE:-$0}" == "$0" ]]; then
  error "perf_setup must be sourced"
  exit 1
fi

function usage() {
  printf "usage: . perf_setup.sh <build_dir>\n"
}

# Ensure that user includes name of build directory (e.g. out/Release) as
# input parameter. Store path in BUILD_DIR.
if [[ "$#" -eq 1 ]]; then
  if is_not_dir "$1"; then
    error "$1 is invalid"
    return 1
  fi
  BUILD_DIR="$1"
else
  error "Missing required parameter".
  usage
  return 1
fi

# Full (relative) path to the libjingle_peerconnection_so.so file.
function native_shared_lib_path() {
  echo "${BUILD_DIR}${NATIVE_LIB_PATH}"
}

# Target CPU architecture for the native shared library.
# Example: AArch64.
function native_shared_lib_arch() {
  readelf -h $(native_shared_lib_path) | grep Machine | awk '{print $2}'
}

# Returns true if the device architecture and the build target are the same.
function arch_is_ok() {
  if [[ "$(dev_arch)" == "aarch64" ]] \
    && [[ "$(native_shared_lib_arch)" == "AArch64" ]]; then
    return 0
  elif [[ "$(dev_arch)" == "aarch32" ]] \
    && [[ "$(native_shared_lib_arch)" == "AArch32" ]]; then
    return 0
  else
    return 1
  fi
}

# Copies the native shared library from the local host to the symbol cache
# which is used by simpleperf as base when searching for symbols.
function copy_native_shared_library_to_symbol_cache() {
  local arm_lib="arm"
  if [[ "$(native_shared_lib_arch)" == "AArch64" ]]; then
    arm_lib="arm64"
  fi
  for num in 1 2; do
    local dir="${SYMBOL_DIR}/data/app/${APP_NAME}-${num}/lib/${arm_lib}"
    mkdir -p "${dir}"
    cp -u $(native_shared_lib_path) "${dir}"
  done
}

# Copy kernel symbols from device to symbol cache in tmp.
function copy_kernel_symbols_from_device_to_symbol_cache() {
  local symbol_cache="${SYMBOL_DIR}/kallsyms"
  adb pull /proc/kallsyms "${symbol_cache}"
} 1> /dev/null

# Download the correct version of 'simpleperf' to $DEV_TMP_DIR
# on the device and enable profiling.
function copy_simpleperf_to_device() {
  local perf_binary
  [[ $(dev_arch) == "aarch64" ]] \
    && perf_binary="/arm64/simpleperf" \
    || perf_binary="/arm/simpleperf"
  # Copy the simpleperf binary from local host to temp folder on device.
  adb push "${SCRIPT_DIR}/simpleperf/bin/android${perf_binary}" \
    "${DEV_TMP_DIR}" 1> /dev/null
  # Copy simpleperf from temp folder to the application package.
  adb shell run-as "${APP_NAME}" cp "${DEV_TMP_DIR}/simpleperf" .
  adb shell run-as "${APP_NAME}" chmod a+x simpleperf
  # Enable profiling on the device.
  enable_profiling
  # Allows usage of running report commands on the device.
  if image_is_root; then
    enable_report_symbols
  fi
}

# Copy the recorded 'perf.data' file from the device to the current directory.
# TODO(henrika): add support for specifying the destination.
function pull_perf_data_from_device() {
  adb shell run-as "${APP_NAME}" cp perf.data /sdcard/perf.data
  adb pull sdcard/perf.data .
} 1> /dev/null


# Wraps calls to simpleperf report. Used by e.g. perf_report_threads.
# A valid profile input file must exist in the current folder.
# TODO(henrika): possibly add support to add path to alternative input file.
function perf_report() {
  local perf_data="perf.data"
  is_file "${perf_data}" \
    && simpleperf report \
      -n \
      -i "${perf_data}" \
      "$@" \
    || error "$(pwd)/${perf_data} is invalid"
}

# Removes the folder specified as input parameter. Mainly intended for removal
# of simpleperf and Flame Graph tools.
function remove_tool() {
  local tool_dir="$1"
  if is_dir "${tool_dir}"; then
    echo "Removing ${tool_dir}..."
    rm -rf "${tool_dir}"
    path_remove "${tool_dir}"
  fi
}

# Utility method which deletes the downloaded simpleperf tool from the repo.
# It also removes the simpleperf root folder from PATH.
function rm_simpleperf() {
  remove_tool "${SCRIPT_DIR}/simpleperf"
}

# Utility method which deletes the downloaded Flame Graph tool from the repo.
# It also removes the Flame Graph root folder from PATH.
function rm_flame_graph() {
  remove_tool "${SCRIPT_DIR}/flamegraph"
}

# Lists the main available functions after sourcing this script.
function print_function_help() {
  printf "\nAvailable functions in this shell:\n"
  printf " perf_record [duration, default=60sec]\n"
  printf " perf_report_threads\n"
  printf " perf_report_bins\n"
  printf " perf_report_symbols\n"
  printf " perf_report_graph\n"
  printf " perf_report_graph_callee\n"
  printf " perf_update\n"
  printf " perf_cleanup\n"
  printf " flame_graph\n"
  printf " plot_flame_graph\n"
}

function cleanup() {
  unset -f main
}

# -----------------------------------------------------------------------------
# Main methods to be used after sourcing the main script.
# -----------------------------------------------------------------------------

# Call this method after the application as been rebuilt and installed on the
# device to ensure that symbols are up-to-date.
function perf_update() {
  copy_native_shared_library_to_symbol_cache
  if image_is_root; then
    copy_kernel_symbols_from_device_to_symbol_cache
  fi
}

# Record stack frame based call graphs while using the application.
# We use default events (cpu-cycles), and write records to 'perf.data' in the
# tmp folder on the device. Default duration is 60 seconds but it can be changed
# by adding one parameter. As soon as the recording is done, 'perf.data' is
# copied to the directory from which this method is called and a summary of
# the load distribution per thread is printed.
function perf_record() {
  if app_is_running "${APP_NAME}"; then
    # Ensure that the latest native shared library exists in the local cache.
    copy_native_shared_library_to_symbol_cache
    local duration=60
    if [ "$#" -eq 1 ]; then
      duration="$1"
    fi
    local pid=$(find_app_pid "${APP_NAME}")
    echo "Profiling PID $pid for $duration seconds (media must be is active)..."
    adb shell run-as "${APP_NAME}" ./simpleperf record \
      --call-graph fp \
      -p "${pid}" \
      -f 1000 \
      --duration "${duration}" \
      --log error
    # Copy profile results from device to current directory.
    pull_perf_data_from_device
    # Print out a summary report (load per thread).
    perf_report_threads | tail -n +6
  else
    # AppRTCMobile was not enabled. Start it up automatically and ask the user
    # to start media and then call this method again.
    warning "AppRTCMobile must be active"
    app_start "${APP_NAME}"
    echo "Start media and then call perf_record again..."
  fi
}

# Analyze the profile report and show samples per threads.
function perf_report_threads() {
  perf_report --sort comm
} 2> /dev/null

# Analyze the profile report and show samples per binary.
function perf_report_bins() {
  perf_report --sort dso
} 2> /dev/null

# Analyze the profile report and show samples per symbol.
function perf_report_symbols() {
  perf_report --sort symbol --symfs "${SYMBOL_DIR}"
}

# Print call graph showing how functions call others.
function perf_report_graph() {
  perf_report -g caller --symfs "${SYMBOL_DIR}"
}

# Print call graph showing how functions are called from others.
function perf_report_graph_callee() {
  perf_report -g callee --symfs "${SYMBOL_DIR}"
}

# Plots the default Flame Graph file if no parameter is provided.
# If a parameter is given, it will be used as file name instead of the default.
function plot_flame_graph() {
  local file_name="flame_graph.svg"
  if [[ "$#" -eq 1 ]]; then
    file_name="$1"
  fi
  # Open up the SVG file in Chrome. Try unstable first and revert to stable
  # if unstable fails.
  google-chrome-unstable "${file_name}" \
    || google-chrome-stable "${file_name}" \
    || error "failed to find any Chrome instance"
} 2> /dev/null

# Generate Flame Graph in interactive SVG format.
# First input parameter corresponds to output file name and second input
# parameter is the heading of the plot.
# Defaults will be utilized if parameters are not provided.
# See https://github.com/brendangregg/FlameGraph for details on Flame Graph.
function flame_graph() {
  local perf_data="perf.data"
  if is_not_file $perf_data; then
    error "$(pwd)/${perf_data} is invalid"
    return 1
  fi
  local file_name="flame_graph.svg"
  local title="WebRTC Flame Graph"
  if [[ "$#" -eq 1 ]]; then
    file_name="$1"
  fi
  if [[ "$#" -eq 2 ]]; then
    file_name="$1"
    title="$2"
  fi
  if image_is_not_root; then
    report_sample.py \
      --symfs "${SYMBOL_DIR}" \
      perf.data >out.perf
  else
    report_sample.py \
      --symfs "${SYMBOL_DIR}" \
      --kallsyms "${SYMBOL_DIR}/kallsyms" \
      perf.data >out.perf
  fi
  stackcollapse-perf.pl out.perf >out.folded
  flamegraph.pl --title="${title}" out.folded >"${file_name}"
  rm out.perf
  rm out.folded
}

# Remove all downloaded third-party tools.
function perf_cleanup () {
  rm_simpleperf
  rm_flame_graph
}

main() {
  printf "%s\n" "Preparing profiling of AppRTCMobile on Android:"
  # Verify that this script is called from the root folder of WebRTC,
  # i.e., the src folder one step below where the .gclient file exists.
  local -r project_root_dir=$(pwd)
  local dir=${project_root_dir##*/}
  if [[ "${dir}" != "src" ]]; then
    error "script must be called from the WebRTC project root (src) folder"
    return 1
  fi
  ok "project root: ${project_root_dir}"

  # Verify that user has sourced envsetup.sh.
  # TODO(henrika): might be possible to remove this check.
  if [[ -z "$ENVSETUP_GYP_CHROME_SRC" ]]; then
    error "must source envsetup script first"
    return 1
  fi
  ok "envsetup script has been sourced"

  # Given that envsetup is sourced, the adb tool should be accessible but
  # do one extra check just in case.
  local adb_full_path=$(which adb);
  if [[ ! -x "${adb_full_path}" ]]; then
    error "unable to find the Android Debug Bridge (adb) tool"
    return 1
  fi
  ok "adb tool is working"

  # Exactly one Android device must be connected.
  if ! one_device_connected; then
    error "one device must be connected"
    return 1
  fi
  ok "one device is connected via USB"

  # Restart adb with root permissions if needed.
  if image_is_root && adb_has_no_root_permissions; then
    adb root
    ok "adb is running as root"
  fi

  # Create an empty symbol cache in the tmp folder.
  # TODO(henrika): it might not be required to start from a clean cache.
  is_dir "${SYMBOL_DIR}" && rm -rf "${SYMBOL_DIR}"
  mkdir "${SYMBOL_DIR}" \
    && ok "empty symbol cache created at ${SYMBOL_DIR}" \
    || error "failed to create symbol cache"

  # Ensure that path to the native library with symbols is valid.
  local native_lib=$(native_shared_lib_path)
  if is_not_file ${native_lib}; then
    error "${native_lib} is not a valid file"
    return 1
  fi
  ok "native library: "${native_lib}""

  # Verify that the architechture of the device matches the architecture
  # of the native library.
  if ! arch_is_ok; then
    error "device is $(dev_arch) and lib is $(native_shared_lib_arch)"
    return 1
  fi
  ok "device is $(dev_arch) and lib is $(native_shared_lib_arch)"

  # Copy native shared library to symbol cache after creating an
  # application specific tree structure under ${SYMBOL_DIR}/data.
  copy_native_shared_library_to_symbol_cache
  ok "native library copied to ${SYMBOL_DIR}/data/app/${APP_NAME}"

  # Verify that the application is installed on the device.
  if ! app_is_installed "${APP_NAME}"; then
    error "${APP_NAME} is not installed on the device"
    return 1
  fi
  ok "${APP_NAME} is installed on the device"

  # Download simpleperf to <src>/tools_webrtc/android/profiling/simpleperf/.
  # Cloning will only take place if the target does not already exist.
  # The PATH variable will also be updated.
  # TODO(henrika): would it be better to use a target outside the WebRTC repo?
  local simpleperf_dir="${SCRIPT_DIR}/simpleperf"
  if is_not_dir "${simpleperf_dir}"; then
    echo "Dowloading simpleperf..."
    git clone https://android.googlesource.com/platform/prebuilts/simpleperf \
      "${simpleperf_dir}"
    chmod u+x "${simpleperf_dir}/report_sample.py"
  fi
  path_add "${simpleperf_dir}"
  ok "${simpleperf_dir}" is added to PATH

  # Update the PATH variable with the path to the Linux version of simpleperf.
  local simpleperf_linux_dir="${SCRIPT_DIR}/simpleperf/bin/linux/x86_64/"
  if is_not_dir "${simpleperf_linux_dir}"; then
    error "${simpleperf_linux_dir} is invalid"
    return 1
  fi
  path_add "${simpleperf_linux_dir}"
  ok "${simpleperf_linux_dir}" is added to PATH

  # Copy correct version (arm or arm64) of simpleperf to the device
  # and enable profiling at the same time.
  if ! copy_simpleperf_to_device; then
    error "failed to install simpleperf on the device"
    return 1
  fi
  ok "simpleperf is installed on the device"

  # Refresh the symbol cache and read kernal symbols from device if not
  # already done.
  perf_update
  ok "symbol cache is updated"

  # Download Flame Graph to <src>/tools_webrtc/android/profiling/flamegraph/.
  # Cloning will only take place if the target does not already exist.
  # The PATH variable will also be updated.
  # TODO(henrika): would it be better to use a target outside the WebRTC repo?
  local flamegraph_dir="${SCRIPT_DIR}/flamegraph"
  if is_not_dir "${flamegraph_dir}"; then
    echo "Dowloading Flame Graph visualization tool..."
    git clone https://github.com/brendangregg/FlameGraph.git "${flamegraph_dir}"
  fi
  path_add "${flamegraph_dir}"
  ok "${flamegraph_dir}" is added to PATH

  print_function_help

  cleanup

  return 0
}

# Only call main() if proper input parameter has been provided.
if is_set $BUILD_DIR; then
  main "$@"
fi

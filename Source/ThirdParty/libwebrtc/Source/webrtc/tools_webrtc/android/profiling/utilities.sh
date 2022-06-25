#!/bin/bash

# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# Utility functions to be used by perf_setup.sh.
# Contains helper methods and functions that wraps usage of adb.

function error() {
  echo "[ERROR] "$@"" >&2
}

function warning() {
  echo "[WARNING] "$@"" >&1
}

function ok() {
  echo "[OK] "$@"" >&1
}

function abs_path {
  (cd $1; pwd)
}

function is_set() {
  local var="$1"
  [[ -n "${var}" ]]
}

function is_file() {
  local file="$1"
  [[ -f "${file}" ]]
}

function is_not_file() {
  local file="$1"
  [[ ! -f "${file}" ]]
}

function is_dir() {
  local dir="$1"
  [[ -d "${dir}" ]]
}

function is_not_dir() {
  local dir="$1"
  [[ ! -d "${dir}" ]]
}

# Adds (prepends) the PATH environment variable while avoid duplicates.
function path_add() {
  case ":${PATH:=$1}:" in
    *:$1:*)  ;;
    *) PATH="$1:$PATH"  ;;
  esac
}

# Removes a path from the PATH environment variable using search-and-replace
# parameter expansion.
function path_remove {
  local path="$1"
  # Substitute first occurrence of ":path" in PATH with an empty string.
  # Deletes instances in the middle or at the end.
  PATH=${PATH/":$path"/}
  # Substitute first occurrence of "path:" in PATH with an empty string.
  # Delete instances at the beginning.
  PATH=${PATH/"$path:"/}
}

# Returns the process ID (PID) of the process that corresponds to the
# application name given as input parameter.
function find_app_pid() {
  local app_name="$1"
  adb shell ps | grep "${app_name}" | awk '{print $2}'
}

function app_is_installed() {
  local app_name="$1"
  local installed_app_name=$(adb shell pm list packages \
    | grep "${app_name}" | awk -F':' '{print $2}')
  is_set "${installed_app_name}" \
    && [[ "${installed_app_name}" = "${app_name}" ]]
}

function app_is_running() {
  local app_name="$1"
  local app_pid=$(find_app_pid "${app_name}")
  is_set "${app_pid}"
}

function app_start() {
  local app_name="$1"
  adb shell am start \
    -n "${app_name}/.ConnectActivity" \
    -a android.intent.action.MAIN
}

function app_stop() {
  local app_name="$1"
  adb shell am force-stop "${app_name}"
}

function app_uninstall() {
  local app_name="$1"
  adb uninstall "${app_name}"
}

function dev_arch() {
  adb shell uname -m
}

function dev_ls() {
  local dir="$1"
  adb shell ls "${dir}"
}

# Returns true if exactly on device is connected.
function one_device_connected() {
  [[ $(adb devices | wc -l) = 3 ]]
}

# Returns true if device is rooted.
function image_is_root() {
  [[ $(adb shell getprop ro.build.type) = "userdebug" ]]
}

# Returns true if device is not rooted.
function image_is_not_root() {
  [[ $(adb shell getprop ro.build.type) = "user" ]]
}

# Returns true if adb is not already running as root.
# Should only be called on rooted devices.
function adb_has_no_root_permissions() {
  [[ $(adb shell getprop service.adb.root) = 0 ]]
}

# Android devices may disable profiling by default. We must enable it.
function enable_profiling() {
  adb shell setprop security.perf_harden 0
}

# To make the report of symbols on device successful, we need to execute
# `echo 0 >/proc/sys/kernel/kptr_restrict`.
# Only needed if we run report commands on the same machine as we run
# record commands.
function enable_report_symbols() {
  adb shell "echo 0 > /proc/sys/kernel/kptr_restrict"
}

#!/bin/bash

# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# 'adb shell' always returns "0" regardless of executable return code.
# This handy script will return executable return code to shell which
# can be used by buildbots.

adb_shell () {
  local RET ADB_LOG
  ADB_LOG=$(mktemp "${TMPDIR:-/tmp}/adb-XXXXXXXX")
  adb "$1" "$2" shell "$3" "$4" ";" echo \$? | tee "$ADB_LOG"
  sed -i -e 's![[:cntrl:]]!!g' "$ADB_LOG"  # Remove \r.
  RET=$(sed -e '$!d' "$ADB_LOG")           # Last line contains status code.
  rm -f "$ADB_LOG"
  return $RET
}

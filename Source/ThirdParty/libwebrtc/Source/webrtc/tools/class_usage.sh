#!/bin/bash

# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This script is run in a git repository. It lists all classes defined
# in header files, sorted by the number of other files where the name
# of the class occurs. It is intentionally not limited to only source
# files. Classes close to the top of the list are candidates for
# removal.

git grep -h '^class .*[:{]' -- '*.h' '*.hpp' \
  | sed -e 's/WEBRTC_DLL_EXPORT// ' -e 's/^class *\([^ :{(<]*\).*/\1/' \
  | sort | uniq | while read class ; do
  count="$(git grep -l -w -F "${class}" | wc -l)"
  echo "${count}" "${class}"
done | sort -n

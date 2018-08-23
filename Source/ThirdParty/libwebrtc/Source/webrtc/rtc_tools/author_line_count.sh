#!/bin/bash

# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This script counts net line count contributions by author. Besides
# amusement, the value of these stats are of course questionable.

git log "$@" --pretty=format:%ae --shortstat \
  | sed '/^ /s/,/\n/g' \
  | gawk '
/^[^ ]/ {
  /* Some author "email addresses" have a trailing @svn-id, strip that out. */
  author = gensub(/^([^@]*@[^@]*).*/, "\\1", "g", $1);
}
/^ .*insertion/ { total[author] += $1 }
/^ .*deletion/ { total[author] -= $1 }
END { for (author in total) {
        print total[author], author
      }
}
' \
  | sort -nr

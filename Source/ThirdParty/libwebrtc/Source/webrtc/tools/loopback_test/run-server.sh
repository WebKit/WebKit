#!/bin/sh
#
# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
#
# This script is used to launch a simple http server for files in the same
# location as the script itself.
cd "`dirname \"$0\"`"
echo "Starting http server in port 8080."
exec python -m SimpleHTTPServer 8080

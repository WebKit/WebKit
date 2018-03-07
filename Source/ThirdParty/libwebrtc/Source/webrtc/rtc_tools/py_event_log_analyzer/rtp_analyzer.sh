#!/bin/sh
#  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.
set -e
WORKING_DIR=$(pwd)
cd $(dirname $0)
PYTHONPATH="../../third_party/protobuf/python/"
if [ -z ${PYTHON_EXECUTABLE+x} ]
then
    PYTHON_EXECUTABLE=python3
fi
exec $PYTHON_EXECUTABLE "rtp_analyzer.py" $@ --working_dir $WORKING_DIR

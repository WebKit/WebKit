#!/bin/bash
###############################################################################
# Copyright 2015 Google Inc.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
###############################################################################
#
# Copies the files identified by the first argument from the host to the target
# path on the device. The path on the devide is relative to the Documents
# directory.
#

set -x -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $SCRIPT_DIR/ios_setup.sh

HOST_PATH=$1
DEVICE_PATH=$2

ios_push $HOST_PATH $DEVICE_PATH

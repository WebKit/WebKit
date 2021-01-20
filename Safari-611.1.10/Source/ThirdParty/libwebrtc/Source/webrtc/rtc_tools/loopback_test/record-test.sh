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
# This script is used to record a tcp dump of running a loop back test.
# Example use case:
#
#  $ ./run-server.sh &       # spawns a server to serve the html pages
#                            # on localhost:8080
#
#  (recording 3 tests with 5mins and bitrates 1mbps, 2mbps and 3mbps)
#  $ sudo -v                              # Caches sudo credentials needed
#                                         # for tcpdump
#  $ export INTERFACE=eth1                # Defines interface to record packets
#  $ export CHROME_UNDER_TESTING=./chrome # Define which chrome to run on tests
#  $ export TEST="http://localhost:8080/loopback_test.html?auto-mode=true"
#  $ record-test.sh ./record1.pcap "$TEST&duration=300&max-video-bitrate=1000"
#  $ record-test.sh ./record2.pcap "$TEST&duration=300&max-video-bitrate=2000"
#  $ record-test.sh ./record3.pcap "$TEST&duration=300&max-video-bitrate=3000"

# Indicate an error and exit with a nonzero status if any of the required
# environment variables is Null or Unset.
: ${INTERFACE:?"Need to set INTERFACE env variable"}
: ${CHROME_UNDER_TESTING:?"Need to set CHROME_UNDER_TESTING env variable"}

if [ ! -x "$CHROME_UNDER_TESTING" ]; then
  echo "CHROME_UNDER_TESTING=$CHROME_UNDER_TESTING does not seem to exist."
  exit 1
fi

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <test-url> <network-dump>"
  exit 1
fi
TEST_URL=$1
OUTPUT_RECORDING=$2

sudo -nv > /dev/null 2>&1
if [ $? != 0 ]; then
  echo "Run \"sudo -v\" to cache your credentials." \
       "They are needed to run tcpdump."
  exit
fi

echo "Recording $INTERFACE into ${OUTPUT_RECORDING}"
sudo -n tcpdump -i "$INTERFACE" -w - > "${OUTPUT_RECORDING}" &
TCPDUMP_PID=$!

echo "Starting ${CHROME_UNDER_TESTING} with ${TEST_URL}."
# Using real camera instead of --use-fake-device-for-media-stream as it
# does not produces images complex enough to reach 3mbps.
# Flag --use-fake-ui-for-media-stream automatically allows getUserMedia calls.
$CHROME_UNDER_TESTING --use-fake-ui-for-media-stream "${TEST_URL}"
kill ${TCPDUMP_PID}

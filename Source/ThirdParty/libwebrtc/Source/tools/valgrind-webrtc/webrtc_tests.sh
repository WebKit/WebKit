#!/bin/bash

# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# Set up some paths and re-direct the arguments to webrtc_tests.py

# This script is a copy of the chrome_tests.sh wrapper script with the following
# changes:
# - The locate_valgrind.sh of Chromium's Valgrind scripts dir is used to locate
#   the Valgrind framework install.
# - webrtc_tests.py is invoked instead of chrome_tests.py.
# - Chromium's Valgrind scripts directory is added to the PYTHONPATH to make it
#   possible to execute the Python scripts properly.

export THISDIR=`dirname $0`
ARGV_COPY="$@"

# We need to set CHROME_VALGRIND iff using Memcheck:
#   tools/valgrind-webrtc/webrtc_tests.sh --tool memcheck
# or
#   tools/valgrind-webrtc/webrtc_tests.sh --tool=memcheck
tool="memcheck"  # Default to memcheck.
while (( "$#" ))
do
  if [[ "$1" == "--tool" ]]
  then
    tool="$2"
    shift
  elif [[ "$1" =~ --tool=(.*) ]]
  then
    tool="${BASH_REMATCH[1]}"
  fi
  shift
done

NEEDS_VALGRIND=0

case "$tool" in
  "memcheck")
    NEEDS_VALGRIND=1
    ;;
esac

# For WebRTC, we'll use the locate_valgrind.sh script in Chromium's Valgrind
# scripts dir to locate the Valgrind framework install
CHROME_VALGRIND_SCRIPTS=$THISDIR/../valgrind

if [ "$NEEDS_VALGRIND" == "1" ]
then
  CHROME_VALGRIND=`sh $CHROME_VALGRIND_SCRIPTS/locate_valgrind.sh`
  if [ "$CHROME_VALGRIND" = "" ]
  then
    # locate_valgrind.sh failed
    exit 1
  fi
  echo "Using valgrind binaries from ${CHROME_VALGRIND}"

  PATH="${CHROME_VALGRIND}/bin:$PATH"
  # We need to set these variables to override default lib paths hard-coded into
  # Valgrind binary.
  export VALGRIND_LIB="$CHROME_VALGRIND/lib/valgrind"
  export VALGRIND_LIB_INNER="$CHROME_VALGRIND/lib/valgrind"

  # Clean up some /tmp directories that might be stale due to interrupted
  # chrome_tests.py execution.
  # FYI:
  #   -mtime +1  <- only print files modified more than 24h ago,
  #   -print0/-0 are needed to handle possible newlines in the filenames.
  echo "Cleanup /tmp from Valgrind stuff"
  find /tmp -maxdepth 1 \(\
        -name "vgdb-pipe-*" -or -name "vg_logs_*" -or -name "valgrind.*" \
      \) -mtime +1 -print0 | xargs -0 rm -rf
fi

# Add Chrome's Valgrind scripts dir to the PYTHON_PATH since it contains
# the scripts that are needed for this script to run
PYTHONPATH=$THISDIR/../python/google:$CHROME_VALGRIND_SCRIPTS python \
           "$THISDIR/webrtc_tests.py" $ARGV_COPY

#!/bin/sh
# Copyright (c) 2010, 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

killAfterTimeout() {
    TARGET_PID=$1
    TIMEOUT=$2
    sleep $TIMEOUT && kill -9 $TARGET_PID &
    WATCHDOG_PID=$!
    wait $TARGET_PID
    # FIXME: This kill will fail (and log) when the watchdog has exited (by killing the child process).
    # There is likely magic shell commands to make it not log (redirecting stderr/stdout didn't work).
    kill -9 $WATCHDOG_PID
}

if [[ $# -ne 2 ]];then
echo "Usage: start-queue.sh QUEUE_NAME BOT_ID"
echo
echo "QUEUE_NAME will be passed as a command to webkit-patch"
echo "QUEUE_NAME will also be used as the path to the queue: /mnt/git/webkit-QUEUE_NAME"
echo "BOT_ID may not have spaces. It will appear as the bots name on queues.webkit.org"
echo
echo "For example, to run the mac-ews on a machine we're calling 'eseidel-cq-sf' run:"
echo "start-queue.sh mac-ews eseidel-cq-sf"
exit 1
fi

QUEUE_NAME=$1
BOT_ID=$2

cd /mnt/git/webkit-$QUEUE_NAME
while :
do
  git reset --hard trunk # Throw away any patches in our tree.
  git clean --force # Remove any left-over layout test results, added files, etc.
  git rebase --abort # If we got killed during a git rebase, we need to clean up.

  # Fetch before we rebase to speed up the rebase (fetching from git.webkit.org is way faster than pulling from svn.webkit.org)
  git fetch
  git svn rebase

  # test-webkitpy has code to remove orphaned .pyc files, so we
  # run it before running webkit-patch to avoid stale .pyc files
  # preventing webkit-patch from launching.
  ./Tools/Scripts/test-webkitpy &
  killAfterTimeout $! $[60*10] # Wait up to 10 minutes for it to run (should take < 30 seconds).

  # We use --exit-after-iteration to pick up any changes to webkit-patch, including
  # changes to the committers.py file.
  # test-webkitpy has been known to hang in the past, so run it using killAfterTimeout
  # See https://bugs.webkit.org/show_bug.cgi?id=57724.
  ./Tools/Scripts/webkit-patch $QUEUE_NAME --bot-id=$BOT_ID --no-confirm --exit-after-iteration 10 &
  killAfterTimeout $! $[60*60*12] # Wait up to 12 hours.
done

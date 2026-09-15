#!/bin/bash
# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.
#
# Build WebKit, keeping the log in a file and the results in an .xcresult bundle. Arguments are
# passed through to build-webkit. Run from the root of a WebKit checkout.

set -o pipefail

if [[ ! -x Tools/Scripts/build-webkit ]]; then
    echo "error: no Tools/Scripts/build-webkit here; run this from the root of a WebKit checkout" >&2
    exit 1
fi

# xcodebuild refuses to write over an existing .xcresult bundle, so use a fresh
# directory per build.
out=$(mktemp -d -t wk-build) || exit 1
echo "xcresult=$out/build.xcresult"
echo "log=$out/build.log"

# Turn off various build sandboxes, as Claude's sandbox is already active. Hide
# hints to use any other plugins.
Tools/Scripts/build-webkit \
    --result-bundle-path="$out/build.xcresult" \
    ENABLE_USER_SCRIPT_SANDBOXING=NO \
    DISABLE_TASK_SANDBOXING=YES \
    OTHER_SWIFT_FLAGS='$(inherited) -disable-sandbox' \
    "$@" 2>&1 | fgrep -v '<claude-code-hint' | tee "$out/build.log" | Tools/Scripts/filter-build-webkit 2>&1
status=$?

echo "exit=$status"
exit $status

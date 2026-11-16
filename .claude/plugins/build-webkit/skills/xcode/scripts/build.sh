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

# Build WebKit, keeping the log in a file and the results in an .xcresult bundle. Arguments are
# passed through to build-webkit. Run from anywhere in a WebKit checkout.

set -o pipefail

root=$PWD
if [[ ! -x $root/Tools/Scripts/build-webkit ]]; then
    root=$(git rev-parse --show-toplevel 2>/dev/null)
fi
if [[ ! -x $root/Tools/Scripts/build-webkit ]]; then
    echo "error: no Tools/Scripts/build-webkit in $PWD or the checkout it is in;" \
         "run this from a WebKit checkout" >&2
    exit 1
fi

# xcodebuild refuses to write over an existing .xcresult bundle, so use a fresh
# directory per build.
out=$(mktemp -d -t wk-build) || exit 1
echo "xcresult=$out/build.xcresult"
echo "log=$out/build.log"

# Test whether the agent is running inside a sandbox. If it is, pass build
# settings to avoid failing due to nested sandbox_apply.
sandbox_off=()
if ! sandbox-exec -p '(version 1)(allow default)' /usr/bin/true >/dev/null 2>&1; then
    sandbox_off=(ENABLE_USER_SCRIPT_SANDBOXING=NO
                 DISABLE_TASK_SANDBOXING=YES
                 OTHER_SWIFT_FLAGS='$(inherited) -disable-sandbox')
fi

echo "Follow the build output as it runs:"
echo "tail -f $out/build.log | $root/Tools/Scripts/filter-build-webkit"

# fgrep: Hide hints to use any other plugins.
"$root/Tools/Scripts/build-webkit" \
    --xcode \
    --result-bundle-path="$out/build.xcresult" \
    "${sandbox_off[@]}" \
    "$@" 2>&1 | fgrep -v '<claude-code-hint' > "$out/build.log"
status=${PIPESTATUS[0]}

echo "exit=$status"
exit $status

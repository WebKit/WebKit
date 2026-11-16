#!/usr/bin/env python3
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
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""Generate a clang VFS overlay YAML that swaps the SDK's
<os/availability.h> and <Availability.h> for WebKit-supplied stubs that
defuse availability checks.
"""

import argparse
import json
import sys
from pathlib import Path

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument("--sdk", type=Path, required=True)
ap.add_argument("--stubs", type=Path, required=True)
ap.add_argument("--out", type=Path, required=True)
args = ap.parse_args()

HEADERS = ("usr/include/os/availability.h", "usr/include/Availability.h")

content = json.dumps({
    "version": 0,
    "case-sensitive": False,
    "roots": [
        {"name": str(args.sdk / rel),
         "type": "file",
         "external-contents": str(args.stubs / rel)}
        for rel in HEADERS
    ],
}, indent=2) + "\n"

# Skip the write when content is unchanged to preserve mtime and avoid
# spurious downstream rebuilds. Writing directly (rather than temp+rename)
# is required for Xcode's user-script sandbox, which only permits writes
# to declared outputPaths.
if args.out.is_file() and args.out.read_text() == content:
    sys.exit(0)
args.out.write_text(content)

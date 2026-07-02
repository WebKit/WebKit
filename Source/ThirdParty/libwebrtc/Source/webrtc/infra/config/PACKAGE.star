#!/usr/bin/env lucicfg

#  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Package declaration for the WebRTC project."""

pkg.declare(
    name = "@webrtc-project",
    lucicfg = "1.45.6",
)

pkg.entrypoint("config.star")

pkg.options.lint_checks([
    "default",
])

pkg.depend(
    name = "@chromium-luci",
    source = pkg.source.googlesource(
        host = "chromium",
        repo = "infra/chromium",
        ref = "refs/heads/main",
        path = "starlark-libs/chromium-luci",
        revision = "04cdad5911e5d091eba42fc1ede0dd08b9261166",
    ),
)

pkg.resources([
    "console-header.textpb",
    "luci-analysis.cfg",
    "templates/*",
])

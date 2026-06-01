#!/usr/bin/env lucicfg
#
# Copyright 2025 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Package declaration for the Angle project."""

pkg.declare(
    name = "@angle-project",
    lucicfg = "1.45.8",
)

pkg.options.lint_checks([
    "all",
])

pkg.entrypoint("main.star")

pkg.depend(
    name = "@chromium-luci",
    source = pkg.source.googlesource(
        host = "chromium",
        repo = "infra/chromium",
        ref = "refs/heads/main",
        path = "starlark-libs/chromium-luci",
        revision = "31c5420c99c320603429ad3bde362ec935cb5c78",
    ),
)

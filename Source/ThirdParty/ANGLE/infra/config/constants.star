# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Constants shared between multiple ANGLE Starlark files."""

# Copied from Chromium's lib/builders.star and trimmed.
siso = struct(
    project = struct(
        DEFAULT_TRUSTED = "rbe-chromium-trusted",
        DEFAULT_UNTRUSTED = "rbe-chromium-untrusted",
    ),
    remote_jobs = struct(
        DEFAULT = 250,
        LOW_JOBS_FOR_CI = 80,
        HIGH_JOBS_FOR_CI = 500,
        LOW_JOBS_FOR_CQ = 150,
    ),
)

default_experiments = {
    # Fail build when merge script fails.
    "chromium_swarming.expose_merge_script_failures": 100,
}

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""ANGLE GN arg definitions."""

load("@chromium-luci//gn_args.star", "gn_args")

gn_args.config(
    name = "capture",
    args = {
        "angle_with_capture_by_default": True,
    },
)

gn_args.config(
    name = "clang",
    args = {
        "is_clang": True,
    },
)

gn_args.config(
    name = "component",
    args = {
        "is_component_build": True,
    },
)

gn_args.config(
    name = "linux",
    args = {
        "target_os": "linux",
    },
)

gn_args.config(
    name = "linux_clang",
    configs = [
        "clang",
        "linux",
        "siso",
    ],
)

gn_args.config(
    name = "minimal_symbols",
    args = {
        "symbol_level": 1,
    },
)

gn_args.config(
    name = "opencl",
    args = {
        "angle_enable_cl": True,
    },
)

gn_args.config(
    name = "release",
    args = {
        "is_debug": False,
    },
    configs = [
        "minimal_symbols",
    ],
)

gn_args.config(
    name = "release_with_dchecks",
    args = {
        "dcheck_always_on": True,
    },
    configs = [
        "release",
    ],
)

gn_args.config(
    name = "siso",
    args = {
        "use_reclient": False,
        "use_remoteexec": True,
        "use_siso": True,
    },
)

gn_args.config(
    name = "x64",
    args = {
        "target_cpu": "x64",
    },
)

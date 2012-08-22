#!/usr/bin/env python
# Copyright (C) 2011 Google Inc. All rights reserved.
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

import re

from webkitpy.common.memoized import memoized


# In this dictionary, each item stores:
# * port_name -- a fully qualified port name
# * specifiers -- a set of specifiers, representing configurations covered by this builder.
_exact_matches = {
    # These builders are on build.chromium.org.
    "Webkit Win": {"port_name": "chromium-win-xp", "specifiers": set(["xp", "release"])},
    "Webkit Win7": {"port_name": "chromium-win-win7", "specifiers": set(["win7"])},
    "Webkit Win (dbg)(1)": {"port_name": "chromium-win-xp", "specifiers": set(["win", "debug"])},
    "Webkit Win (dbg)(2)": {"port_name": "chromium-win-xp", "specifiers": set(["win", "debug"])},
    "Webkit Linux": {"port_name": "chromium-linux-x86_64", "specifiers": set(["linux", "x86_64", "release"])},
    "Webkit Linux 32": {"port_name": "chromium-linux-x86", "specifiers": set(["linux", "x86"])},
    "Webkit Linux (dbg)": {"port_name": "chromium-linux-x86_64", "specifiers": set(["linux", "debug"])},
    "Webkit Mac10.6": {"port_name": "chromium-mac-snowleopard", "specifiers": set(["snowleopard"])},
    "Webkit Mac10.6 (dbg)": {"port_name": "chromium-mac-snowleopard", "specifiers": set(["snowleopard", "debug"])},
    "Webkit Mac10.7": {"port_name": "chromium-mac-lion", "specifiers": set(["lion"])},

    # These builders are on build.webkit.org.
    "Apple MountainLion Release WK1 (Tests)": {"port_name": "mac-mountainlion", "specifiers": set(["mountainlion"]), "rebaseline_override_dir": "mac"},
    "Apple MountainLion Debug WK1 (Tests)": {"port_name": "mac-mountainlion", "specifiers": set(["mountainlion", "debug"]), "rebaseline_override_dir": "mac"},
    "Apple MountainLion Release WK2 (Tests)": {"port_name": "mac-mountainlion", "specifiers": set(["mountainlion", "wk2"]), "rebaseline_override_dir": "mac"},
    "Apple MountainLion Debug WK2 (Tests)": {"port_name": "mac-mountainlion", "specifiers": set(["mountainlion", "wk2", "debug"]), "rebaseline_override_dir": "mac"},
    "Apple Lion Release WK1 (Tests)": {"port_name": "mac-lion", "specifiers": set(["lion"])},
    "Apple Lion Debug WK1 (Tests)": {"port_name": "mac-lion", "specifiers": set(["lion", "debug"])},
    "Apple Lion Release WK2 (Tests)": {"port_name": "mac-lion", "specifiers": set(["lion", "wk2"])},
    "Apple Lion Debug WK2 (Tests)": {"port_name": "mac-lion", "specifiers": set(["lion", "wk2", "debug"])},

    "Apple Win XP Debug (Tests)": {"port_name": "win-xp", "specifiers": set(["win", "debug"])},
    # FIXME: Remove rebaseline_override_dir once there is an Apple buildbot that corresponds to platform/win.
    "Apple Win 7 Release (Tests)": {"port_name": "win-7sp0", "specifiers": set(["win"]), "rebaseline_override_dir": "win"},

    "GTK Linux 32-bit Release": {"port_name": "gtk", "specifiers": set(["gtk", "x86", "release"])},
    "GTK Linux 64-bit Debug": {"port_name": "gtk", "specifiers": set(["gtk", "x86_64", "debug"])},
    "GTK Linux 64-bit Release": {"port_name": "gtk", "specifiers": set(["gtk", "x86_64", "release"])},
    "GTK Linux 64-bit Release WK2 (Tests)": {"port_name": "gtk", "specifiers": set(["gtk", "x86_64", "wk2", "release"])},

    # FIXME: Remove rebaseline_override_dir once there are Qt bots for all the platform/qt-* directories.
    "Qt Linux Release": {"port_name": "qt-linux", "specifiers": set(["win", "linux", "mac"]), "rebaseline_override_dir": "qt"},

    "EFL Linux 64-bit Debug": {"port_name": "efl", "specifiers": set(["efl", "debug"])},
    "EFL Linux 64-bit Release": {"port_name": "efl", "specifiers": set(["efl", "release"])},
}


_fuzzy_matches = {
    # These builders are on build.webkit.org.
    r"SnowLeopard": "mac-snowleopard",
    r"Apple Lion": "mac-lion",
    r"Windows": "win",
    r"GTK": "gtk",
    r"Qt": "qt",
    r"Chromium Mac": "chromium-mac",
    r"Chromium Linux": "chromium-linux",
    r"Chromium Win": "chromium-win",
}


_ports_without_builders = [
    "qt-mac",
    "qt-win",
    "qt-wk2",
]


def builder_path_from_name(builder_name):
    return re.sub(r'[\s().]', '_', builder_name)


def all_builder_names():
    return sorted(set(_exact_matches.keys()))


def all_port_names():
    return sorted(set(map(lambda x: x["port_name"], _exact_matches.values()) + _ports_without_builders))


def coverage_specifiers_for_builder_name(builder_name):
    return _exact_matches[builder_name].get("specifiers", set())


def rebaseline_override_dir(builder_name):
    return _exact_matches[builder_name].get("rebaseline_override_dir", None)


def port_name_for_builder_name(builder_name):
    if builder_name in _exact_matches:
        return _exact_matches[builder_name]["port_name"]

    for regexp, port_name in _fuzzy_matches.items():
        if re.match(regexp, builder_name):
            return port_name


def builder_name_for_port_name(target_port_name):
    for builder_name, builder_info in _exact_matches.items():
        if builder_info['port_name'] == target_port_name and 'debug' not in builder_info['specifiers']:
            return builder_name
    return None


def builder_path_for_port_name(port_name):
    builder_path_from_name(builder_name_for_port_name(port_name))


def fallback_port_names_for_new_port(builder_name):
    return _exact_matches[builder_name].get("move_overwritten_baselines_to", [])

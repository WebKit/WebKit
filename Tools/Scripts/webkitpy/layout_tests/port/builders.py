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
    "Webkit Win": {"port_name": "chromium-win-xp", "specifiers": set(["xp", "release", "cpu"])},
    "Webkit Vista": {"port_name": "chromium-win-vista", "specifiers": set(["vista"])},
    "Webkit Win7": {"port_name": "chromium-win-win7", "specifiers": set(["win7", "cpu"])},
    "Webkit Win (dbg)(1)": {"port_name": "chromium-win-xp", "specifiers": set(["win", "debug"])},
    "Webkit Win (dbg)(2)": {"port_name": "chromium-win-xp", "specifiers": set(["win", "debug"])},
    "Webkit Linux": {"port_name": "chromium-linux-x86_64", "specifiers": set(["linux", "x86_64", "release"])},
    "Webkit Linux 32": {"port_name": "chromium-linux-x86", "specifiers": set(["linux", "x86"])},
    "Webkit Linux (dbg)(1)": {"port_name": "chromium-linux-x86_64", "specifiers": set(["linux", "debug"])},
    "Webkit Linux (dbg)(2)": {"port_name": "chromium-linux-x86_64", "specifiers": set(["linux", "debug"])},
    "Webkit Mac10.5": {"port_name": "chromium-mac-leopard", "specifiers": set(["leopard"])},
    "Webkit Mac10.5 (dbg)(1)": {"port_name": "chromium-mac-leopard", "specifiers": set(["leopard", "debug"])},
    "Webkit Mac10.5 (dbg)(2)": {"port_name": "chromium-mac-leopard", "specifiers": set(["leopard", "debug"])},
    "Webkit Mac10.6": {"port_name": "chromium-mac-snowleopard", "specifiers": set(["snowleopard"])},
    "Webkit Mac10.6 (dbg)": {"port_name": "chromium-mac-snowleopard", "specifiers": set(["snowleopard", "debug"])},
    "Webkit Win - GPU": {"port_name": "chromium-gpu-win-xp", "specifiers": set(["xp", "release", "gpu"])},
    "Webkit Win7 - GPU": {"port_name": "chromium-gpu-win-win7", "specifiers": set(["win7", "vista", "release", "gpu"])},
    # FIXME: For some reason, these port names don't work correctly.
    # "Webkit Linux - GPU": {"port_name": "chromium-gpu-linux-x86_64", "specifiers": set(["linux", "gpu"])},
    # "Webkit Linux 32 - GPU": {"port_name": "chromium-gpu-linux-x86", "specifiers": set(["linux", "x86", "gpu"])},
    "Webkit Mac10.6 (dbg) - GPU": {"port_name": "chromium-gpu-mac-snowleopard", "specifiers": set(["snowleopard", "gpu", "debug"])},
    "Webkit Mac10.6 - GPU": {"port_name": "chromium-gpu-mac-snowleopard", "specifiers": set(["snowleopard", "gpu"])},

    # These builders are on build.webkit.org.
    "GTK Linux 32-bit Debug": {"port_name": "gtk", "specifiers": set(["gtk"])},
    "Leopard Intel Debug (Tests)": {"port_name": "mac-leopard", "specifiers": set(["leopard"])},
    "SnowLeopard Intel Release (Tests)": {"port_name": "mac-snowleopard", "specifiers": set(["snowleopard"])},
    "SnowLeopard Intel Release (WebKit2 Tests)": {"port_name": "mac-wk2", "specifiers": set(["wk2"])},
    "Qt Linux Release": {"port_name": "qt-linux", "specifiers": set(["win", "linux", "mac"])},
    "Windows XP Debug (Tests)": {"port_name": "win-xp", "specifiers": set(["win"])},
    "Windows 7 Release (WebKit2 Tests)": {"port_name": "win-wk2", "specifiers": set(["wk2"])},
}


_fuzzy_matches = {
    # These builders are on build.webkit.org.
    r"SnowLeopard": "mac-snowleopard",
    r"Leopard": "mac-leopard",
    r"Windows": "win",
    r"GTK": "gtk",
    r"Qt": "qt",
    r"Chromium Mac": "chromium-mac",
    r"Chromium Linux": "chromium-linux",
    r"Chromium Win": "chromium-win",
}


_ports_without_builders = [
    # FIXME: Including chromium-gpu-linux below is a workaround for
    # chromium-gpu-linux-x86_64 and chromium-gpu-linux-x86 not working properly.
    "chromium-gpu-linux",
    "google-chrome-linux32",
    "google-chrome-linux64",
    "qt-mac",
    "qt-win",
    "qt-wk2",
]


def builder_path_from_name(builder_name):
    return re.sub(r'[\s().]', '_', builder_name)


@memoized
def all_builder_names():
    return sorted(set(_exact_matches.keys()))


@memoized
def all_port_names():
    return sorted(set(map(lambda x: x["port_name"], _exact_matches.values()) + _ports_without_builders))


def coverage_specifiers_for_builder_name(builder_name):
    return _exact_matches[builder_name].get("specifiers", set())


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

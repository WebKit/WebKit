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


_exact_matches = {
    # These builders are on build.chromium.org.
    "Webkit Win": "chromium-win-xp",
    "Webkit Vista": "chromium-win-vista",
    "Webkit Win7": "chromium-win-win7",
    "Webkit Win (dbg)(1)": "chromium-win-xp",
    "Webkit Win (dbg)(2)": "chromium-win-xp",
    "Webkit Linux": "chromium-linux-x86_64",
    "Webkit Linux 32": "chromium-linux-x86",
    "Webkit Linux (dbg)(1)": "chromium-linux-x86_64",
    "Webkit Linux (dbg)(2)": "chromium-linux-x86_64",
    "Webkit Mac10.5": "chromium-mac-leopard",
    "Webkit Mac10.5 (dbg)(1)": "chromium-mac-leopard",
    "Webkit Mac10.5 (dbg)(2)": "chromium-mac-leopard",
    "Webkit Mac10.6": "chromium-mac-snowleopard",
    "Webkit Mac10.6 (dbg)": "chromium-mac-snowleopard",
    "Webkit Mac10.6 - GPU": "chromium-gpu-mac-snowleopard",
    "Webkit Win - GPU": "chromium-gpu-win-xp",
    "Webkit Win7 - GPU": "chromium-gpu-win-win7",
    # FIXME: For some reason, these port names don't work correctly.
    # "Webkit Linux - GPU": "chromium-gpu-linux-x86_64",
    # "Webkit Linux 32 - GPU": "chromium-gpu-linux-x86",
    "Webkit Mac10.5 - GPU": "chromium-gpu-mac-leopard",
    "Webkit Mac10.6 - GPU": "chromium-gpu-mac-snowleopard",

    # These builders are on build.webkit.org.
    "GTK Linux 32-bit Debug": "gtk",
    "Leopard Intel Debug (Tests)": "mac-leopard",
    "SnowLeopard Intel Release (Tests)": "mac-snowleopard",
    "SnowLeopard Intel Release (WebKit2 Tests)": "mac-wk2",
    "Qt Linux Release": "qt-linux",
    "Windows XP Debug (Tests)": "win-xp",
    "Windows 7 Release (WebKit2 Tests)": "win-wk2",
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
    # FIXME: Including chromium-gpu-linux below is a workaroudn for
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
def all_port_names():
    return sorted(set(_exact_matches.values() + _ports_without_builders))


def port_name_for_builder_name(builder_name):
    if builder_name in _exact_matches:
        return _exact_matches[builder_name]

    for regexp, port_name in _fuzzy_matches.items():
        if re.match(regexp, builder_name):
            return port_name


def builder_name_for_port_name(target_port_name):
    for builder_name, port_name in _exact_matches.items():
        if port_name == target_port_name:
            return builder_name
    return None


def builder_path_for_port_name(port_name):
    builder_path_from_name(builder_name_for_port_name(port_name))

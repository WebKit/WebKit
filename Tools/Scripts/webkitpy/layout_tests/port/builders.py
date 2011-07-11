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


def builder_path_from_name(builder_name):
    return re.sub(r'[\s().]', '_', builder_name)


# Compiled manually from http://build.chromium.org/p/chromium/json/builders/help?as_text=1
# Values of None mean there are no bots running at build.webkit.org or
# build.chromium.org for that port.
# FIXME Make the values in this map into lists.
CHROMIUM_PORT_TO_BUILDER_NAME = {
    'chromium-gpu-linux': builder_path_from_name('Webkit Linux - GPU'),

    'chromium-gpu-mac-snowleopard': builder_path_from_name('Webkit Mac10.6 - GPU'),
    'chromium-gpu-mac-leopard': builder_path_from_name('Webkit Mac10.5 - GPU'),

    'chromium-gpu-win-xp': builder_path_from_name('Webkit Win - GPU'),
    'chromium-gpu-win-vista': builder_path_from_name('Webkit Vista - GPU'),
    'chromium-gpu-win-win7': builder_path_from_name('Webkit Win7 - GPU'),

    'chromium-linux-x86_64': builder_path_from_name('Linux Tests x64'),
    'chromium-linux-x86': builder_path_from_name('Linux Tests (dbg)(1)'),

    'chromium-mac-leopard': builder_path_from_name('Mac10.5 Tests (1)'),
    'chromium-mac-snowleopard': builder_path_from_name('Mac 10.6 Tests (dbg)(1)'),

    'chromium-win-xp': builder_path_from_name('XP Tests (dbg)(5)'),
    'chromium-win-vista': builder_path_from_name('Vista Tests (dbg)(1)'),
    'chromium-win-win7': None,

    'google-chrome-linux32': None,
    'google-chrome-linux64': None,
}

# Compiled manually from http://build.webkit.org/builders
WEBKIT_PORT_TO_BUILDER_NAME = {
    'gtk': 'GTK Linux 32-bit Debug',

    'mac-leopard': 'Leopard Intel Debug (Tests)',
    'mac-snowleopard': 'SnowLeopard Intel Release (Tests)',
    'mac-wk2': 'SnowLeopard Intel Release (WebKit2 Tests)',

    'qt-linux': 'Qt Linux Release',
    'qt-mac': None,
    'qt-win': None,
    'qt-wk2': None,

    'win-xp': 'Windows XP Debug (Tests)',
    'win': None,
    'win-wk2': 'Windows 7 Release (WebKit2 Tests)',
}

PORT_TO_BUILDER_NAME = {}
PORT_TO_BUILDER_NAME.update(CHROMIUM_PORT_TO_BUILDER_NAME)
PORT_TO_BUILDER_NAME.update(WEBKIT_PORT_TO_BUILDER_NAME)


def builder_name_for_platform(platform):
    return PORT_TO_BUILDER_NAME.get(platform)

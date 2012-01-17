#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
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

import sys

import chromium_linux
import chromium_mac
import chromium_win


# FIXME: This whole file needs to go away and we need to eliminate the GPU configuration.

def _set_gpu_options(port, graphics_type='gpu'):
    port._graphics_type = graphics_type
    if port.get_option('accelerated_2d_canvas') is None:
        port._options.accelerated_2d_canvas = True
    if port.get_option('accelerated_video') is None:
        port._options.accelerated_video = True
    if port.get_option('experimental_fully_parallel') is None:
        port._options.experimental_fully_parallel = True

    # FIXME: Remove this after http://codereview.chromium.org/5133001/ is enabled
    # on the bots.
    if port.get_option('builder_name') is not None and not ' - GPU' in port._options.builder_name:
        port._options.builder_name += ' - GPU'


def _default_tests_paths(port):
    paths = []
    if port.name() != 'chromium-gpu-mac-leopard':
        # Only run tests requiring accelerated compositing on platforms that
        # support it.
        # FIXME: we should add the above paths here as well but let's test
        # the waters with media first.
        paths += ['media']

    paths += ['fast/canvas', 'canvas/philip']

    if not paths:
        # FIXME: This is a hack until we can turn off the webkit_gpu
        # tests on the bots. If paths is empty, port.tests()
        # finds *everything*. However, we have to return something,
        # or NRWT thinks there's something wrong. So, we return a single
        # short directory. See https://bugs.webkit.org/show_bug.cgi?id=72498.
        paths = ['fast/html']

    return paths


class ChromiumGpuLinuxPort(chromium_linux.ChromiumLinuxPort):
    port_name = 'chromium-gpu-linux'

    def __init__(self, host, port_name, **kwargs):
        chromium_linux.ChromiumLinuxPort.__init__(self, host, port_name, **kwargs)
        _set_gpu_options(self)

    def baseline_search_path(self):
        # Mimic the Linux -> Win expectations fallback in the ordinary Chromium port.
        return (map(self._webkit_baseline_path, ['chromium-gpu-linux', 'chromium-gpu-win', 'chromium-gpu']) +
                chromium_linux.ChromiumLinuxPort.baseline_search_path(self))

    def tests(self, paths):
        paths = paths or _default_tests_paths(self)
        return chromium_linux.ChromiumLinuxPort.tests(self, paths)


class ChromiumGpuMacPort(chromium_mac.ChromiumMacPort):
    port_name = 'chromium-gpu-mac'

    def __init__(self, host, port_name, **kwargs):
        chromium_mac.ChromiumMacPort.__init__(self, host, port_name, **kwargs)
        _set_gpu_options(self)

    def baseline_search_path(self):
        return (map(self._webkit_baseline_path, ['chromium-gpu-mac', 'chromium-gpu']) +
                chromium_mac.ChromiumMacPort.baseline_search_path(self))

    def tests(self, paths):
        paths = paths or _default_tests_paths(self)
        return chromium_mac.ChromiumMacPort.tests(self, paths)


class ChromiumGpuWinPort(chromium_win.ChromiumWinPort):
    port_name = 'chromium-gpu-win'

    def __init__(self, host, port_name, **kwargs):
        chromium_win.ChromiumWinPort.__init__(self, host, port_name, **kwargs)
        _set_gpu_options(self)

    def baseline_search_path(self):
        return (map(self._webkit_baseline_path, ['chromium-gpu-win', 'chromium-gpu']) +
                chromium_win.ChromiumWinPort.baseline_search_path(self))

    def tests(self, paths):
        paths = paths or _default_tests_paths(self)
        return chromium_win.ChromiumWinPort.tests(self, paths)

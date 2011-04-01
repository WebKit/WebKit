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

from webkitpy.layout_tests.port import test_files


def get(platform=None, port_name='chromium-gpu', **kwargs):
    """Some tests have slightly different results when run while using
    hardware acceleration.  In those cases, we prepend an additional directory
    to the baseline paths."""
    platform = platform or sys.platform
    if port_name == 'chromium-gpu':
        if platform in ('cygwin', 'win32'):
            port_name = 'chromium-gpu-win'
        elif platform == 'linux2':
            port_name = 'chromium-gpu-linux'
        elif platform == 'darwin':
            port_name = 'chromium-gpu-mac'
        else:
            raise NotImplementedError('unsupported platform: %s' % platform)

    if port_name.startswith('chromium-gpu-linux'):
        return ChromiumGpuLinuxPort(port_name=port_name, **kwargs)
    if port_name.startswith('chromium-gpu-mac'):
        return ChromiumGpuMacPort(port_name=port_name, **kwargs)
    if port_name.startswith('chromium-gpu-win'):
        return ChromiumGpuWinPort(port_name=port_name, **kwargs)
    raise NotImplementedError('unsupported port: %s' % port_name)


# FIXME: These should really be a mixin class.

def _set_gpu_options(port):
    port._graphics_type = 'gpu'
    if port.get_option('accelerated_compositing') is None:
        port._options.accelerated_compositing = True
    if port.get_option('accelerated_2d_canvas') is None:
        port._options.accelerated_2d_canvas = True

    # FIXME: Remove this after http://codereview.chromium.org/5133001/ is enabled
    # on the bots.
    if port.get_option('builder_name') is not None and not ' - GPU' in port._options.builder_name:
        port._options.builder_name += ' - GPU'


def _tests(port, paths):
    if not paths:
        paths = ['compositing', 'platform/chromium/compositing', 'media']
        if not port.name().startswith('chromium-gpu-mac'):
            # Canvas is not yet accelerated on the Mac, so there's no point
            # in running the tests there.
            paths += ['fast/canvas', 'canvas/philip']
        # invalidate_rect.html tests a bug in the compositor.
        # See https://bugs.webkit.org/show_bug.cgi?id=53117
        paths += ['plugins/invalidate_rect.html']
    return test_files.find(port, paths)


class ChromiumGpuLinuxPort(chromium_linux.ChromiumLinuxPort):
    def __init__(self, port_name='chromium-gpu-linux', **kwargs):
        chromium_linux.ChromiumLinuxPort.__init__(self, port_name=port_name, **kwargs)
        _set_gpu_options(self)

    def baseline_path(self):
        # GPU baselines aren't yet versioned.
        return self._webkit_baseline_path('chromium-gpu-linux')

    def baseline_search_path(self):
        # Mimic the Linux -> Win expectations fallback in the ordinary Chromium port.
        return (map(self._webkit_baseline_path, ['chromium-gpu-linux', 'chromium-gpu-win', 'chromium-gpu']) +
                chromium_linux.ChromiumLinuxPort.baseline_search_path(self))

    def default_child_processes(self):
        return 1

    def tests(self, paths):
        return _tests(self, paths)


class ChromiumGpuMacPort(chromium_mac.ChromiumMacPort):
    def __init__(self, port_name='chromium-gpu-mac', **kwargs):
        chromium_mac.ChromiumMacPort.__init__(self, port_name=port_name, **kwargs)
        _set_gpu_options(self)

    def baseline_path(self):
        # GPU baselines aren't yet versioned.
        return self._webkit_baseline_path('chromium-gpu-mac')

    def baseline_search_path(self):
        return (map(self._webkit_baseline_path, ['chromium-gpu-mac', 'chromium-gpu']) +
                chromium_mac.ChromiumMacPort.baseline_search_path(self))

    def default_child_processes(self):
        return 1

    def tests(self, paths):
        return _tests(self, paths)


class ChromiumGpuWinPort(chromium_win.ChromiumWinPort):
    def __init__(self, port_name='chromium-gpu-win', **kwargs):
        chromium_win.ChromiumWinPort.__init__(self, port_name=port_name, **kwargs)
        _set_gpu_options(self)

    def baseline_path(self):
        # GPU baselines aren't yet versioned.
        return self._webkit_baseline_path('chromium-gpu-win')

    def baseline_search_path(self):
        return (map(self._webkit_baseline_path, ['chromium-gpu-win', 'chromium-gpu']) +
                chromium_win.ChromiumWinPort.baseline_search_path(self))

    def default_child_processes(self):
        return 1

    def tests(self, paths):
        return _tests(self, paths)

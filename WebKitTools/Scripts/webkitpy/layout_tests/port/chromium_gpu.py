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

import codecs
import os
import sys

import chromium_linux
import chromium_mac
import chromium_win


def get(**kwargs):
    """Some tests have slightly different results when run while using
    hardware acceleration.  In those cases, we prepend an additional directory
    to the baseline paths."""
    port_name = kwargs.get('port_name', None)
    if port_name == 'chromium-gpu':
        if sys.platform in ('cygwin', 'win32'):
            port_name = 'chromium-gpu-win'
        elif sys.platform == 'linux2':
            port_name = 'chromium-gpu-linux'
        elif sys.platform == 'darwin':
            port_name = 'chromium-gpu-mac'
        else:
            raise NotImplementedError('unsupported platform: %s' %
                                      sys.platform)

    if port_name == 'chromium-gpu-linux':
        return ChromiumGpuLinuxPort(**kwargs)

    if port_name.startswith('chromium-gpu-mac'):
        return ChromiumGpuMacPort(**kwargs)

    if port_name.startswith('chromium-gpu-win'):
        return ChromiumGpuWinPort(**kwargs)

    raise NotImplementedError('unsupported port: %s' % port_name)


def _set_gpu_options(options):
    if options:
        if options.accelerated_compositing is None:
            options.accelerated_composting = True
        if options.accelerated_2d_canvas is None:
            options.accelerated_2d_canvas = True


def _gpu_overrides(port):
    try:
        overrides_path = port.path_from_chromium_base('webkit', 'tools',
            'layout_tests', 'test_expectations_gpu.txt')
    except AssertionError:
        return None
    if not os.path.exists(overrides_path):
        return None
    with codecs.open(overrides_path, "r", "utf-8") as file:
        return file.read()


class ChromiumGpuLinuxPort(chromium_linux.ChromiumLinuxPort):
    def __init__(self, **kwargs):
        kwargs.setdefault('port_name', 'chromium-gpu-linux')
        _set_gpu_options(kwargs.get('options'))
        chromium_linux.ChromiumLinuxPort.__init__(self, **kwargs)

    def baseline_search_path(self):
        return ([self._webkit_baseline_path('chromium-gpu-linux')] +
                chromium_linux.ChromiumLinuxPort.baseline_search_path(self))

    def path_to_test_expectations_file(self):
        return self.path_from_webkit_base('LayoutTests', 'platform',
            'chromium-gpu', 'test_expectations.txt')

    def test_expectations_overrides(self):
        return _gpu_overrides(self)


class ChromiumGpuMacPort(chromium_mac.ChromiumMacPort):
    def __init__(self, **kwargs):
        kwargs.setdefault('port_name', 'chromium-gpu-mac')
        _set_gpu_options(kwargs.get('options'))
        chromium_mac.ChromiumMacPort.__init__(self, **kwargs)

    def baseline_search_path(self):
        return ([self._webkit_baseline_path('chromium-gpu-mac')] +
                chromium_mac.ChromiumMacPort.baseline_search_path(self))

    def path_to_test_expectations_file(self):
        return self.path_from_webkit_base('LayoutTests', 'platform',
            'chromium-gpu', 'test_expectations.txt')

    def test_expectations_overrides(self):
        return _gpu_overrides(self)


class ChromiumGpuWinPort(chromium_win.ChromiumWinPort):
    def __init__(self, **kwargs):
        kwargs.setdefault('port_name', 'chromium-gpu-win' + self.version())
        _set_gpu_options(kwargs.get('options'))
        chromium_win.ChromiumWinPort.__init__(self, **kwargs)

    def baseline_search_path(self):
        return ([self._webkit_baseline_path('chromium-gpu-win')] +
                chromium_win.ChromiumWinPort.baseline_search_path(self))

    def path_to_test_expectations_file(self):
        return self.path_from_webkit_base('LayoutTests', 'platform',
            'chromium-gpu', 'test_expectations.txt')

    def test_expectations_overrides(self):
        return _gpu_overrides(self)

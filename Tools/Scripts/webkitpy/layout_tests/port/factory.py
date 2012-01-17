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

"""Factory method to retrieve the appropriate port implementation."""

import re

from webkitpy.layout_tests.port import builders


class BuilderOptions(object):
    def __init__(self, builder_name):
        self.configuration = "Debug" if re.search(r"[d|D](ebu|b)g", builder_name) else "Release"
        self.builder_name = builder_name


class PortFactory(object):
    def __init__(self, host):
        self._host = host

    def _port_name_from_arguments_and_options(self, port_name, options, platform):
        if port_name == 'chromium-gpu':
            if platform.is_win():
                return 'chromium-gpu-win'
            if platform.is_linux():
                return 'chromium-gpu-linux'
            if platform.is_mac():
                return 'chromium-gpu-mac'

        if port_name:
            return port_name

        if platform.is_win():
            if options and hasattr(options, 'chromium') and options.chromium:
                return 'chromium-win'
            return 'win'
        if platform.is_linux():
            return 'chromium-linux'
        if platform.is_mac():
            if options and hasattr(options, 'chromium') and options.chromium:
                return 'chromium-mac'
            return 'mac'

        raise NotImplementedError('unknown port; os_name = "%s"' % platform.os_name)

    def get(self, port_name=None, options=None, **kwargs):
        """Returns an object implementing the Port interface. If
        port_name is None, this routine attempts to guess at the most
        appropriate port on this platform."""
        port_to_use = self._port_name_from_arguments_and_options(port_name, options, self._host.platform)
        port_name = port_name or port_to_use
        if port_to_use.startswith('test'):
            import test
            maker = test.TestPort
        elif port_to_use.startswith('dryrun'):
            import dryrun
            maker = dryrun.DryRunPort
        elif port_to_use.startswith('mock-'):
            import mock_drt
            maker = mock_drt.MockDRTPort
        elif port_to_use.startswith('mac'):
            import mac
            maker = mac.MacPort
        elif port_to_use.startswith('win'):
            import win
            maker = win.WinPort
        elif port_to_use.startswith('gtk'):
            import gtk
            maker = gtk.GtkPort
        elif port_to_use.startswith('qt'):
            import qt
            maker = qt.QtPort
        elif port_to_use.startswith('chromium-gpu-linux'):
            import chromium_gpu
            port_name = port_to_use
            maker = chromium_gpu.ChromiumGpuLinuxPort
        elif port_to_use.startswith('chromium-gpu-mac'):
            import chromium_gpu
            port_name = port_to_use
            maker = chromium_gpu.ChromiumGpuMacPort
        elif port_to_use.startswith('chromium-gpu-win'):
            import chromium_gpu
            port_name = port_to_use
            maker = chromium_gpu.ChromiumGpuWinPort
        elif port_to_use.startswith('chromium-mac'):
            import chromium_mac
            port_name = port_to_use
            maker = chromium_mac.ChromiumMacPort
        elif port_to_use.startswith('chromium-linux'):
            import chromium_linux
            maker = chromium_linux.ChromiumLinuxPort
        elif port_to_use.startswith('chromium-win'):
            import chromium_win
            maker = chromium_win.ChromiumWinPort
        elif port_to_use == 'google-chrome-linux32':
            import google_chrome
            port_name = 'chromium-linux-x86'
            maker = google_chrome.GoogleChromeLinux32Port
        elif port_to_use == 'google-chrome-linux64':
            import google_chrome
            port_name = 'chromium-linux-x86_64'
            maker = google_chrome.GoogleChromeLinux64Port
        elif port_to_use.startswith('google-chrome-win'):
            import google_chrome
            maker = google_chrome.GoogleChromeWinPort
        elif port_to_use.startswith('google-chrome-mac'):
            import google_chrome
            maker = google_chrome.GoogleChromeMacPort
        elif port_to_use.startswith('efl'):
            import efl
            maker = efl.EflPort
        else:
            raise NotImplementedError('unsupported port: %s' % port_to_use)

        port_name = maker.determine_full_port_name(self._host, options, port_name)
        return maker(self._host, port_name, options=options, **kwargs)

    def all_port_names(self):
        """Return a list of all valid, fully-specified, "real" port names.

        This is the list of directories that are used as actual baseline_paths()
        by real ports. This does not include any "fake" names like "test"
        or "mock-mac", and it does not include any directories that are not ."""
        # FIXME: There's probably a better way to generate this list ...
        return builders.all_port_names()

    def get_from_builder_name(self, builder_name):
        port_name = builders.port_name_for_builder_name(builder_name)
        assert(port_name)  # Need to update port_name_for_builder_name
        port = self.get(port_name, BuilderOptions(builder_name))
        assert(port)  # Need to update port_name_for_builder_name
        return port

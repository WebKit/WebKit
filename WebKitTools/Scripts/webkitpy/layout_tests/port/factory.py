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


import sys

ALL_PORT_NAMES = ['test', 'dryrun', 'mac', 'win', 'gtk', 'qt', 'chromium-mac',
                  'chromium-linux', 'chromium-win', 'google-chrome-win',
                  'google-chrome-mac', 'google-chrome-linux32', 'google-chrome-linux64']


def get(port_name=None, options=None, **kwargs):
    """Returns an object implementing the Port interface. If
    port_name is None, this routine attempts to guess at the most
    appropriate port on this platform."""
    # Wrapped for backwards-compatibility
    if port_name:
        kwargs['port_name'] = port_name
    if options:
        kwargs['options'] = options
    return _get_kwargs(**kwargs)


def _get_kwargs(**kwargs):
    port_to_use = kwargs.get('port_name', None)
    options = kwargs.get('options', None)
    if port_to_use is None:
        if sys.platform == 'win32' or sys.platform == 'cygwin':
            if options and hasattr(options, 'chromium') and options.chromium:
                port_to_use = 'chromium-win'
            else:
                port_to_use = 'win'
        elif sys.platform == 'linux2':
            port_to_use = 'chromium-linux'
        elif sys.platform == 'darwin':
            if options and hasattr(options, 'chromium') and options.chromium:
                port_to_use = 'chromium-mac'
            else:
                port_to_use = 'mac'

    if port_to_use is None:
        raise NotImplementedError('unknown port; sys.platform = "%s"' %
                                  sys.platform)

    if port_to_use == 'test':
        import test
        cls = test.TestPort
    elif port_to_use.startswith('dryrun'):
        import dryrun
        cls = dryrun.DryRunPort
    elif port_to_use.startswith('mac'):
        import mac
        cls = mac.MacPort
    elif port_to_use.startswith('win'):
        import win
        cls = win.WinPort
    elif port_to_use.startswith('gtk'):
        import gtk
        cls = gtk.GtkPort
    elif port_to_use.startswith('qt'):
        import qt
        cls = qt.QtPort
    elif port_to_use.startswith('chromium-mac'):
        import chromium_mac
        cls = chromium_mac.ChromiumMacPort
    elif port_to_use.startswith('chromium-linux'):
        import chromium_linux
        cls = chromium_linux.ChromiumLinuxPort
    elif port_to_use.startswith('chromium-win'):
        import chromium_win
        cls = chromium_win.ChromiumWinPort
    elif port_to_use.startswith('google-chrome'):
        import google_chrome
        cls = google_chrome.GetGoogleChromePort
    else:
        raise NotImplementedError('unsupported port: %s' % port_to_use)
    return cls(**kwargs)

def get_all(options=None):
    """Returns all the objects implementing the Port interface."""
    return dict([(port_name, get(port_name, options=options))
                 for port_name in ALL_PORT_NAMES])

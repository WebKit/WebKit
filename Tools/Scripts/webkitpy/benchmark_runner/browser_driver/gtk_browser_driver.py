# Copyright (C) 2016 Igalia S.L. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
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

import logging
import subprocess
from browser_driver import BrowserDriver

_log = logging.getLogger(__name__)


class GTKBrowserDriver(BrowserDriver):
    process_name = None
    platform = 'gtk'

    def prepare_env(self, device_id):
        self.close_browsers()

    def restore_env(self):
        pass

    def close_browsers(self):
        self._terminate_processes(self.process_name)

    @classmethod
    def _launch_process(cls, args, env=None):
        process = subprocess.Popen(args)
        return process

    @classmethod
    def _terminate_processes(cls, process_name):
        _log.info('Closing all processes with name %s' % process_name)
        subprocess.call(['/usr/bin/killall', process_name])

    @classmethod
    def _screen_size(cls):
        # load_subclasses() from __init__.py will load this file to
        # check the platform defined. Do here a lazy import instead of
        # trying to import the Gtk module on the global scope of this
        # file to avoid ImportError errors on other platforms.
        # Python imports are cached and only run once, so this should be ok.
        from gi.repository import Gtk
        screen = Gtk.Window().get_screen()
        return screen.get_monitor_geometry(screen.get_primary_monitor())

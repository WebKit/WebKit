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
import sys
from gtk_browser_driver import GTKBrowserDriver

_log = logging.getLogger(__name__)


class GTKMiniBrowserDriver(GTKBrowserDriver):
    process_name = 'MiniBrowser'
    browser_name = 'minibrowser'

    def prepare_env(self, device_id):
        self._minibrowser_process = None
        super(GTKMiniBrowserDriver, self).prepare_env(device_id)

    def launch_url(self, url, browser_build_path):
        args = ['Tools/Scripts/run-minibrowser', '--gtk']
        args.append("--geometry=%sx%s" % (self._screen_size().width, self._screen_size().height))
        args.append(url)
        _log.info('Launching Minibrowser with url: %s' % url)
        self._minibrowser_process = GTKBrowserDriver._launch_process(args)

    def close_browsers(self):
        super(GTKMiniBrowserDriver, self).close_browsers()
        if self._minibrowser_process and self._minibrowser_process.returncode:
            sys.exit('MiniBrowser crashed with exitcode %d' % self._minibrowser_process.returncode)

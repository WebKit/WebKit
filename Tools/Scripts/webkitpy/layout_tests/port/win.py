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
#     * Neither the Google name nor the names of its
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

"""WebKit Win implementation of the Port interface."""

import logging

from webkitpy.layout_tests.port.webkit import WebKitPort


_log = logging.getLogger(__name__)


class WinPort(WebKitPort):
    port_name = "win"

    FALLBACK_PATHS = {
        'win7': [
            "win",
            "mac-snowleopard",
            "mac",
        ],
    }

    def __init__(self, **kwargs):
        WebKitPort.__init__(self, **kwargs)
        self._version = 'win7'
        self._operating_system = 'win'

    def baseline_search_path(self):
        # Based on code from old-run-webkit-tests expectedDirectoryForTest()
        # FIXME: This does not work for WebKit2.
        return map(self._webkit_baseline_path, self.FALLBACK_PATHS[self._version])

    # FIXME: webkitperl/httpd.pm installs /usr/lib/apache/libphp4.dll on cycwin automatically
    # as part of running old-run-webkit-tests.  That's bad design, but we may need some similar hack.
    # We might use setup_environ_for_server for such a hack (or modify apache_http_server.py).

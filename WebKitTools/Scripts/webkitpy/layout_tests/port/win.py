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
import os
import subprocess

from webkitpy.layout_tests.port.webkit import WebKitPort

_log = logging.getLogger("webkitpy.layout_tests.port.win")


class WinPort(WebKitPort):
    """WebKit Win implementation of the Port class."""

    def __init__(self, port_name=None, options=None):
        if port_name is None:
            port_name = 'win'
        WebKitPort.__init__(self, port_name, options)

    def version(self):
        return ''

    def _tests_for_other_platforms(self):
        # FIXME: This list could be dynamic based on platform name and
        # pushed into base.Port.
        # This really need to be automated.
        return [
            "platform/chromium",
            "platform/gtk",
            "platform/qt",
            "platform/mac",
        ]

    # FIXME: Validate this list.
    def _tests_for_disabled_features(self):
        # FIXME: This should use the feature detection from
        # webkitperl/features.pm to match run-webkit-tests.
        # For now we hard-code a list of features known to be disabled on
        # the Win platform.
        disabled_feature_tests = [
            "fast/xhtmlmp",
            "http/tests/wml",
            "mathml",
            "wml",
        ]
        # FIXME: webarchive tests expect to read-write from
        # -expected.webarchive files instead of .txt files.
        # This script doesn't know how to do that yet, so pretend they're
        # just "disabled".
        webarchive_tests = [
            "webarchive",
            "svg/webarchive",
            "http/tests/webarchive",
            "svg/custom/image-with-prefix-in-webarchive.svg",
        ]
        return disabled_feature_tests + webarchive_tests

    # FIXME: This doesn't have anything to do with WebKit.
    # FIXME: Copy/pasted from chromium-win
    def _kill_process(self, pid):
        """Forcefully kill the process.

        Args:
        pid: The id of the process to be killed.
        """
        subprocess.call(('taskkill.exe', '/f', '/pid', str(pid)),
                        stdin=open(os.devnull, 'r'),
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE)

    def _path_to_apache(self):
        raise NotImplemented

    def _path_to_apache_config_file(self):
        raise NotImplemented

    def _shut_down_http_server(self, server_pid):
        raise NotImplemented

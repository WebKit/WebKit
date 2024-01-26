# Copyright 2016 The Chromium Authors. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

"""Utility functions used both when importing and exporting."""

import json
import logging
import os


WPT_GH_ORG = os.environ.get('WPT_GH_ORG', 'web-platform-tests')
WPT_GH_REPO_NAME = os.environ.get('WPT_GH_REPO_NAME', 'wpt')
WPT_GH_URL = 'https://github.com/%s/%s' % (WPT_GH_ORG, WPT_GH_REPO_NAME)

TEMPLATED_TEST_HEADER = '<!-- This file is required for WebKit test infrastructure to run the templated test -->'

_log = logging.getLogger(__name__)


def read_credentials(host, credentials_json):
    """Extracts credentials from a JSON file."""
    if not credentials_json:
        return {}
    if not host.filesystem.exists(credentials_json):
        _log.warning('Credentials JSON file not found at %s.', credentials_json)
        return {}
    credentials = {}
    contents = json.loads(host.filesystem.read_text_file(credentials_json))
    for key in ('GH_USER', 'GH_TOKEN', 'GERRIT_USER', 'GERRIT_TOKEN'):
        if key in contents:
            credentials[key] = contents[key]
    return credentials


class WPTPaths:
    CHECKOUT_DIRECTORY = ["w3c-tests"]
    WPT_CHECKOUT_PATH = CHECKOUT_DIRECTORY + ["web-platform-tests"]

    @staticmethod
    def checkout_directory(finder):
        return finder.path_from_webkit_outputdir(*WPTPaths.CHECKOUT_DIRECTORY)

    @staticmethod
    def wpt_checkout_path(finder):
        return finder.path_from_webkit_outputdir(*WPTPaths.WPT_CHECKOUT_PATH)

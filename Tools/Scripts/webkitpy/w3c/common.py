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
from pathlib import PurePath

from webkitcorepy import Terminal, run
from webkitscmpy import local

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
    @staticmethod
    def checkout_directory(finder):
        return os.path.dirname(WPTPaths.default_wpt_checkout_path(finder))

    @staticmethod
    def default_wpt_checkout_path(finder):
        return os.path.join(os.path.dirname(finder.webkit_base()), "wpt")

    def ensure_wpt_repository(finder, repository_directory=None, *, non_interactive=True):
        if not repository_directory:
            repository_directory = WPTPaths.default_wpt_checkout_path(finder)

        repository_directory = finder._filesystem.abspath(repository_directory)
        d = PurePath(repository_directory)

        if finder._filesystem.isdir(repository_directory):
            if finder._filesystem.isfile(str(d / "resources" / "testharness.js")) and finder._filesystem.isfile(str(d / "wpt")):
                _log.info(f'Using the WPT repository found at `{repository_directory}`')
                return repository_directory
        else:
            _log.info(f'The default WPT repository location is `{repository_directory}`.')
            if not non_interactive:
                user_directory = Terminal.input(f'Press `Enter` to use the default or input a directory of your choice: ')
                repository_directory = user_directory.strip() or repository_directory

        if run([local.Git.executable(), 'clone', f'{WPT_GH_URL}.git', repository_directory]).returncode:
            return None
        return repository_directory

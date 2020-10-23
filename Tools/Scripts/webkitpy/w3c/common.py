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
WPT_MIRROR_URL = 'https://chromium.googlesource.com/external/w3c/web-platform-tests.git'
WPT_GH_SSH_URL_TEMPLATE = 'https://{}@github.com/%s/%s.git' % (WPT_GH_ORG, WPT_GH_REPO_NAME)
WPT_REVISION_FOOTER = 'WPT-Export-Revision:'
EXPORT_PR_LABEL = 'chromium-export'
PROVISIONAL_PR_LABEL = 'do not merge yet'

# TODO(qyearsley): Avoid hard-coding third_party/WebKit/LayoutTests.
CHROMIUM_WPT_DIR = 'third_party/WebKit/LayoutTests/external/wpt/'

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


def is_testharness_baseline(filename):
    """Checks whether a given file name appears to be a testharness baseline.

    Args:
        filename: A path (absolute or relative) or a basename.
    """
    return filename.endswith('-expected.txt')


def is_basename_skipped(basename):
    """Checks whether to skip (not sync) a file based on its basename.

    Note: this function is used during both import and export, i.e., files with
    skipped basenames are never imported or exported.
    """
    assert '/' not in basename
    blacklist = [
        'MANIFEST.json',    # MANIFEST.json is automatically regenerated.
        'OWNERS',           # https://crbug.com/584660 https://crbug.com/702283
        'reftest.list',     # https://crbug.com/582838
    ]
    return (basename in blacklist
            or is_testharness_baseline(basename)
            or basename.startswith('.'))


def is_file_exportable(path):
    """Checks whether a file in Chromium WPT should be exported to upstream.

    Args:
        path: A relative path from the root of Chromium repository.
    """
    assert path.startswith(CHROMIUM_WPT_DIR)
    basename = path[path.rfind('/') + 1:]
    return not is_basename_skipped(basename)


class WPTPaths:
    CHECKOUT_DIRECTORY = ["w3c-tests"]
    WPT_CHECKOUT_PATH = CHECKOUT_DIRECTORY + ["web-platform-tests"]

    @staticmethod
    def checkout_directory(finder):
        return finder.path_from_webkit_outputdir(*WPTPaths.CHECKOUT_DIRECTORY)

    @staticmethod
    def wpt_checkout_path(finder):
        return finder.path_from_webkit_outputdir(*WPTPaths.WPT_CHECKOUT_PATH)

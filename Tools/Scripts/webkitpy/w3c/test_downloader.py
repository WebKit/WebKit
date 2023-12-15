# Copyright (c) 2014, Canon Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
# 3.  Neither the name of Canon Inc. nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""
 This script downloads W3C test repositories.
"""

import json
import logging

from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.common.checkout.scm.git import Git

_log = logging.getLogger(__name__)


class TestDownloader(object):

    @staticmethod
    def default_options():
        options = type('', (), {})
        options.fetch = True
        options.verbose = False
        options.import_all = False
        return options

    @staticmethod
    def load_test_repositories(filesystem=None):
        return [
            {
                "name": "web-platform-tests",
                "url": "https://github.com/web-platform-tests/wpt.git",
                "revision": "0313d9f",
                "paths_to_skip": [
                    "conformance-checkers",
                    "docs",
                    "old-tests",
                    "resources/testharness.css",
                    "resources/testharnessreport.js",
                ],
                "paths_to_import": [
                    "common",
                    "config.default.json",
                    "fonts",
                    "images",
                    "resources",
                    "serve.py",
                ],
                "import_options": ["generate_init_py"],
            }
        ]

    def __init__(self, repository_directory, port, options):
        self._options = options
        self._port = port
        self._host = port.host
        self._filesystem = port.host.filesystem
        self._test_suites = []

        self.repository_directory = repository_directory
        self.upstream_revision = None

        self.test_repositories = self.load_test_repositories(self._filesystem)

        self.paths_to_skip = []
        self.paths_to_import = []
        for test_repository in self.test_repositories:
            self.paths_to_skip.extend([self._filesystem.join(test_repository['name'], path) for path in test_repository['paths_to_skip']])
            self.paths_to_import.extend([self._filesystem.join(test_repository['name'], path) for path in test_repository['paths_to_import']])

        webkit_finder = WebKitFinder(self._filesystem)
        self.import_expectations_path = port.path_from_webkit_base('LayoutTests', 'imported', 'w3c', 'resources', 'import-expectations.json')
        if not self._filesystem.isfile(self.import_expectations_path):
            _log.warning('Unable to read import expectation file: %s' % self.import_expectations_path)
        if not self._options.import_all:
            self._init_paths_from_expectations()

    def git(self, test_repository):
        return Git(test_repository, None, executive=self._host.executive, filesystem=self._filesystem)

    def checkout_test_repository(self, revision, url, directory):
        git = None
        if not self._filesystem.exists(directory):
            _log.info('Cloning %s into %s...' % (url, directory))
            Git.clone(url, directory, self._host.executive)
            git = self.git(directory)
        elif self._options.fetch is True:
            git = self.git(directory)
            _log.info('Fetching %s...' % url)
            git.fetch()
        else:
            git = self.git(directory)
        _log.info('Checking out revision ' + revision)
        git.checkout(revision, not self._options.verbose)
        self.upstream_revision = git.rev_parse('HEAD')

    def _init_paths_from_expectations(self):
        import_lines = json.loads(self._filesystem.read_text_file(self.import_expectations_path))
        for path, policy in import_lines.items():
            if policy == 'skip':
                self.paths_to_skip.append(path)
            elif policy == 'import':
                self.paths_to_import.append(path)
            else:
                _log.warning('Problem reading import lines ' + path)

    def update_import_expectations(self, test_paths):
        import_lines = json.loads(self._filesystem.read_text_file(self.import_expectations_path))
        for path in test_paths:
            stripped_path = path.rstrip(self._filesystem.sep)
            path_segs = stripped_path.split("/")

            already_imported = False
            for i in range(len(path_segs) - 1, 1, -1):
                parent = path_segs[:i]
                parent_expectation = import_lines.get("/".join(parent))
                if parent_expectation == "import":
                    already_imported = True
                if parent_expectation:
                    break
            if not already_imported:
                import_lines[stripped_path] = "import"

        self._filesystem.write_text_file(self.import_expectations_path, json.dumps(import_lines, sort_keys=True, indent=4, separators=(',', ': ')) + "\n")

    def clone_tests(self, use_tip_of_tree=False):
        for test_repository in self.test_repositories:
            self.checkout_test_repository(test_repository['revision'] if not use_tip_of_tree else 'origin/master', test_repository['url'], self._filesystem.join(self.repository_directory, test_repository['name']))

    def download_tests(self, use_tip_of_tree=False):
        self.clone_tests(use_tip_of_tree)

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
    def load_test_repositories(filesystem=FileSystem()):
        webkit_finder = WebKitFinder(filesystem)
        test_repositories_path = webkit_finder.path_from_webkit_base('LayoutTests', 'imported', 'w3c', 'resources', 'TestRepositories')
        return json.loads(filesystem.read_text_file(test_repositories_path))

    def __init__(self, repository_directory, host, options):
        self._options = options
        self._host = host
        self._filesystem = host.filesystem
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
        self.import_expectations_path = webkit_finder.path_from_webkit_base('LayoutTests', 'imported', 'w3c', 'resources', 'import-expectations.json')
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
            import_lines[path.rstrip(self._filesystem.sep)] = 'import'
        self._filesystem.write_text_file(self.import_expectations_path, json.dumps(import_lines, sort_keys=True, indent=4, separators=(',', ': ')))

    def _git_submodules_description(self, test_repository):
        directory = self._filesystem.join(self.repository_directory, test_repository['name'])

        git = self.git(directory)
        git.init_submodules()

        submodules = []
        submodules_status = [line.strip().split(' ') for line in git.submodules_status().splitlines()]
        for status in submodules_status:
            version = status[0]
            path = status[1].split('/')

            url = self.git(self._filesystem.join(directory, status[1])).origin_url()
            if not url.startswith('https://github.com/'):
                _log.warning('Submodule %s (%s) is not hosted on github' % (status[1], url))
                _log.warning('Please ensure that generated URL points to an archive of the module or manually edit its value after the import')
            url = url[:-4]  # to remove .git

            submodule = {}
            submodule['path'] = path
            submodule['url'] = url + '/archive/' + version + '.tar.gz'
            submodule['url_subpath'] = url.split('/').pop() + '-' + version
            submodules.append(submodule)

        git.deinit_submodules()
        return submodules

    def generate_git_submodules_description(self, test_repository, filepath):
        self._filesystem.write_text_file(filepath, json.dumps(self._git_submodules_description(test_repository), sort_keys=True, indent=4))

    def generate_gitignore(self, test_repository, destination_directory):
        rules = []
        for submodule in self._git_submodules_description(test_repository):
            path = list(submodule['path'])
            path.insert(0, '')
            rules.append('/'.join(path[:-1]) + '/' + path[-1] + '/')
            rules.append('/'.join(path[:-1]) + '/.' + path[-1] + '.url')
        self._filesystem.write_text_file(self._filesystem.join(destination_directory, test_repository['name'], '.gitignore'), '\n'.join(rules))

    def clone_tests(self, use_tip_of_tree=False):
        for test_repository in self.test_repositories:
            self.checkout_test_repository(test_repository['revision'] if not use_tip_of_tree else 'origin/master', test_repository['url'], self._filesystem.join(self.repository_directory, test_repository['name']))

    def download_tests(self, use_tip_of_tree=False):
        self.clone_tests(use_tip_of_tree)

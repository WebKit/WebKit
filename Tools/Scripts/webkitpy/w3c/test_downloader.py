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
#FIXME: Implement submodules extraction to prepare for generation of LayoutTests/imported/w3C/resources/WPTModules

import json
import logging

from webkitpy.common.system.executive import Executive
from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.layout_tests.models.test_expectations import TestExpectationParser
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

        self.test_repositories = self.load_test_repositories(self._filesystem)

        self.paths_to_skip = []
        self.paths_to_import = []
        for test_repository in self.test_repositories:
            self.paths_to_skip.extend([self._filesystem.join(test_repository['name'], path) for path in test_repository['paths_to_skip']])
            self.paths_to_import.extend([self._filesystem.join(test_repository['name'], path) for path in test_repository['paths_to_import']])

        if not self._options.import_all:
            webkit_finder = WebKitFinder(self._filesystem)
            import_expectations_path = webkit_finder.path_from_webkit_base('LayoutTests', 'imported', 'w3c', 'resources', 'ImportExpectations')
            self._init_paths_from_expectations(import_expectations_path)

    def git(self, test_repository):
        return Git(test_repository, None, executive=self._host.executive, filesystem=self._filesystem)

    def checkout_test_repository(self, revision, url, directory):
        git = self.git('.')
        if not self._filesystem.exists(directory):
            _log.info('Cloning %s into %s...' % (url, directory))
            git._run_git(['clone', '-v', url, directory])
        elif self._options.fetch is True:
            _log.info('Fetching %s...' % url)
            git._run_git(['-C', directory, 'fetch'])
        _log.info('Checking out revision ' + revision)
        checkout_arguments = ['-C', directory, 'checkout', revision]
        if not self._options.verbose:
            checkout_arguments += ['-q']
        git._run_git(checkout_arguments)

    def _init_paths_from_expectations(self, file_path):
        if not self._filesystem.isfile(file_path):
            _log.warning('Unable to read import expectation file: %s' % file_path)
            return
        parser = TestExpectationParser(self._host.port_factory.get(), (), False)
        for line in parser.parse(file_path, self._filesystem.read_text_file(file_path)):
            if 'SKIP' in line.modifiers:
                self.paths_to_skip.append(line.name)
            elif 'PASS' in line.expectations:
                self.paths_to_import.append(line.name)

    def _add_test_suite_paths(self, test_paths, directory, webkit_path):
        for name in self._filesystem.listdir(directory):
            path = self._filesystem.join(webkit_path, name)
            if not name.startswith('.') and not path in self.paths_to_skip:
                test_paths.append(path)

    def _empty_directory(self, directory):
        if self._filesystem.exists(directory):
            self._filesystem.rmtree(directory)
        self._filesystem.maybe_make_directory(directory)

    def copy_tests(self, destination_directory, test_paths):
        for test_repository in self.test_repositories:
            self._empty_directory(self._filesystem.join(destination_directory, test_repository['name']))

        copy_paths = []
        if test_paths:
            copy_paths.extend(test_paths)
            copy_paths.extend(self.paths_to_import)
        else:
            for test_repository in self.test_repositories:
                self._add_test_suite_paths(copy_paths, self._filesystem.join(self.repository_directory, test_repository['name']), test_repository['name'])
            # Handling of tests marked as [ Pass ] in expectations file.
            for path in self.paths_to_import:
                if not path in copy_paths:
                    copy_paths.append(path)

        for path in copy_paths:
            source_path = self._filesystem.join(self.repository_directory, path)
            destination_path = self._filesystem.join(destination_directory, path)
            if not self._filesystem.exists(source_path):
                _log.error('Cannot copy %s' % source_path)
            elif self._filesystem.isdir(source_path):
                self._filesystem.copytree(source_path, destination_path)
            else:
                self._filesystem.maybe_make_directory(self._filesystem.dirname(destination_path))
                self._filesystem.copyfile(source_path, destination_path)

        for path in self.paths_to_skip:
            destination_path = self._filesystem.join(destination_directory, path)
            if self._filesystem.isfile(destination_path):
                self._filesystem.remove(destination_path)
            elif self._filesystem.isdir(destination_path):
                self._filesystem.rmtree(destination_path)

    def _git_submodules_status(self, repository_directory):
        return self.git(repository_directory)._run_git(['submodule', 'status'])

    def _git_submodules_description(self, test_repository):
        submodules = []
        repository_directory = self._filesystem.join(self.repository_directory, test_repository['name'])
        if self._filesystem.isfile(self._filesystem.join(repository_directory, '.gitmodules')):
            submodule = {}
            for line in self._filesystem.read_text_file(self._filesystem.join(repository_directory, '.gitmodules')).splitlines():
                line = line.strip()
                if line.startswith('path = '):
                    submodule['path'] = line[7:]
                elif line.startswith('url = '):
                    submodule['url'] = line[6:]
                    if not submodule['url'].startswith('https://github.com/'):
                        _log.warning('Submodule %s is not hosted on github' % submodule['path'])
                        _log.warning('Please ensure that generated URL points to an archive of the module or manually edit its value after the import')
                    submodules.append(submodule)
                    submodule = {}

        submodules_status = [line[1:].split(' ') for line in self._git_submodules_status(repository_directory).splitlines()]
        for submodule in submodules:
            for status in submodules_status:
                if submodule['path'] == status[1]:
                    url = submodule['url'][:-4]
                    version = status[0]
                    repository_name = url.split('/').pop()
                    submodule['url'] = url + '/archive/' + version + '.tar.gz'
                    submodule['url_subpath'] = repository_name + '-' + version
            if '/' in submodule['path']:
                steps = submodule['path'].split('/')
                submodule['name'] = steps.pop()
                submodule['path'] = steps
            else:
                submodule['name'] = submodule['path']
                submodule['path'] = ['.']
        return submodules

    def generate_git_submodules_description(self, test_repository, filepath):
        self._filesystem.write_text_file(filepath, json.dumps(self._git_submodules_description(test_repository), sort_keys=True, indent=4))

    def download_tests(self, destination_directory, test_paths=[]):
        for test_repository in self.test_repositories:
            self.checkout_test_repository(test_repository['revision'], test_repository['url'], self._filesystem.join(self.repository_directory, test_repository['name']))
        self.copy_tests(destination_directory, test_paths)

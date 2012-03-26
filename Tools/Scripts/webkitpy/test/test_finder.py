# Copyright (C) 2012 Google, Inc.
# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""this module is responsible for finding python tests."""

import logging
import sys


_log = logging.getLogger(__name__)


class TestDirectoryTree(object):
    def __init__(self, filesystem, top_directory, starting_subdirectory):
        self.filesystem = filesystem
        self.top_directory = filesystem.realpath(top_directory)
        self.search_directory = self.top_directory
        self.top_package = ''
        if starting_subdirectory:
            self.top_package = starting_subdirectory.replace(filesystem.sep, '.') + '.'
            self.search_directory = filesystem.join(self.top_directory, starting_subdirectory)

    def find_modules(self, suffixes):

        def file_filter(filesystem, dirname, basename):
            return any(basename.endswith(suffix) for suffix in suffixes)

        filenames = self.filesystem.files_under(self.search_directory, file_filter=file_filter)
        return [self.to_module(filename) for filename in filenames]

    def to_module(self, path):
        return path.replace(self.top_directory + self.filesystem.sep, '').replace(self.filesystem.sep, '.')[:-3]

    def clean(self):
        """Delete all .pyc files in the tree that have no matching .py file."""
        _log.debug("Cleaning orphaned *.pyc files from: %s" % self.search_directory)
        filenames = self.filesystem.files_under(self.search_directory)
        for filename in filenames:
            if filename.endswith(".pyc") and filename[:-1] not in filenames:
                _log.info("Deleting orphan *.pyc file: %s" % filename)
                self.filesystem.remove(filename)


class TestFinder(object):
    def __init__(self, filesystem):
        self.filesystem = filesystem
        self.trees = []

    def add_tree(self, top_directory, starting_subdirectory=None):
        self.trees.append(TestDirectoryTree(self.filesystem, top_directory, starting_subdirectory))

    def additional_paths(self, paths):
        return [tree.top_directory for tree in self.trees if tree.top_directory not in paths]

    def clean_trees(self):
        for tree in self.trees:
            tree.clean()

    def is_module(self, name):
        relpath = name.replace('.', self.filesystem.sep) + '.py'
        return any(self.filesystem.exists(self.filesystem.join(tree.top_directory, relpath)) for tree in self.trees)

    def to_module(self, path):
        for tree in self.trees:
            if path.startswith(tree.top_directory):
                return tree.to_module(path)
        return None

    def find_names(self, args, skip_integrationtests, find_all):
        if args:
            return args

        suffixes = ['_unittest.py']
        if not skip_integrationtests:
            suffixes.append('_integrationtest.py')

        modules = []
        for tree in self.trees:
            modules.extend(tree.find_modules(suffixes))
        modules.sort()

        for module in modules:
            _log.debug("Found: %s" % module)

        # FIXME: Figure out how to move this to test-webkitpy in order to to make this file more generic.
        if not find_all:
            slow_tests = ('webkitpy.common.checkout.scm.scm_unittest',)
            self._exclude(modules, slow_tests, 'are really, really slow', 31818)

            if sys.platform == 'win32':
                win32_blacklist = ('webkitpy.common.checkout',
                                   'webkitpy.common.config',
                                   'webkitpy.tool')
                self._exclude(modules, win32_blacklist, 'fail horribly on win32', 54526)

        return modules

    def _exclude(self, modules, module_prefixes, reason, bugid):
        _log.info('Skipping tests in the following modules or packages because they %s:' % reason)
        for prefix in module_prefixes:
            _log.info('    %s' % prefix)
            modules_to_exclude = filter(lambda m: m.startswith(prefix), modules)
            for m in modules_to_exclude:
                if len(modules_to_exclude) > 1:
                    _log.debug('        %s' % m)
                modules.remove(m)
        _log.info('    (https://bugs.webkit.org/show_bug.cgi?id=%d; use --all to include)' % bugid)
        _log.info('')

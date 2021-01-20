# Copyright (C) 2018 Apple Inc. All rights reserved.
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

import os
import re
import sublime
import sublime_plugin
import subprocess

global s_settings

def plugin_loaded():
    global s_settings
    s_settings = Settings()

def plugin_unloaded():
    s_settings.unload()

class Settings(object):
    def __init__(self):
        self._settings = sublime.load_settings('CopyWebKitPermalink.sublime-settings')
        self._cache = {}

    def unload(self):
        for key in self._cache:
            self._settings.clear_on_change(key)

    @property
    def include_revision(self):
        return self._get('include_revision', default=True)

    @include_revision.setter
    def include_revision(self, value):
        self._set('include_revision', value)

    def _get(self, key, default=None):
        if key not in self._cache:
            self._settings.add_on_change(key, lambda : self._cache.pop(key, None))
            self._cache[key] = self._settings.get(key, default)
        return self._cache[key]

    def _set(self, key, value):
        if key in self._cache and self._cache[key] == value:
            return

        self._cache[key] = value
        self._settings.set(key, value)

class CopyWebKitPermalinkCommand(sublime_plugin.TextCommand):
    def run(self, edit, annotate_blame=False):
        if not self.is_enabled():
            return

        document_path = self.view.file_name()
        self._last_svn_info = None
        self._directory_in_checkout = document_path if os.path.isdir(document_path) else os.path.dirname(document_path)
        self.determine_vcs_from_path(document_path)

        if s_settings.include_revision and not self.path_is_in_webkit_checkout(document_path):
            return

        line_number, _ = self.view.rowcol(self.view.sel()[0].begin())  # Zero-based
        line_number = line_number + 1

        path = self.path_relative_to_repository_root_for_path(document_path)
        revision_info = self.revision_info_for_path(document_path)
        sublime.set_clipboard(self.permalink_for_path(path, line_number, revision_info, annotate_blame))

    def is_enabled(self):
        return len(self.view.sel()) > 0 and bool(self.view.file_name())

    def is_visible(self):
        return self.is_enabled()

    def description(self):
        return 'Copy WebKit Permalink'

    def determine_vcs_from_path(self, path):
        if not os.path.isdir(path):
            path = os.path.dirname(path)
        self._is_svn = False
        self._is_git = False
        self._is_git_svn = False
        if self.is_svn_directory(path):
            self._is_svn = True
            return
        if self.is_git_svn_directory(path):
            self._is_git = True
            self._is_git_svn = True
            return
        if self.is_git_directory(path):
            self._is_git = True
            return

    def path_is_in_webkit_checkout(self, path):
        repository_url = self.revision_info_for_path(path).get('repository_url', '')
        return bool(re.match(r'\w+:\/\/\w+\.webkit.org', repository_url))

    def git_path_relative_to_repository_root_for_path(self, path):
        return subprocess.check_output(['git', 'ls-tree', '--full-name', '--name-only', 'HEAD', path], cwd=self._directory_in_checkout).decode('utf-8').rstrip()

    def svn_path_relative_to_repository_root_for_path(self, path):
        return self.svn_info_for_path(path)['path']

    def path_relative_to_repository_root_for_path(self, path):
        if self._is_svn:
            return self.svn_path_relative_to_repository_root_for_path(path)
        if self._is_git:
            return self.git_path_relative_to_repository_root_for_path(path)
        return ''

    def revision_info_for_path(self, path):
        if s_settings.include_revision:
            if self._is_svn or self._is_git_svn:
                return self.svn_revision_info_for_path(path)
            if self._is_git:
                return self.git_revision_info_for_path(path)
        return {}

    def svn_revision_info_for_path(self, path):
        svn_info = self.svn_info_for_path(path)
        return {'branch': svn_info['branch'], 'revision': svn_info['revision'], 'repository_url': svn_info['repositoryRoot']}

    def git_revision_info_for_path(self, path):
        repository_url = subprocess.check_output(['git', 'remote', 'get-url', 'origin'], cwd=self._directory_in_checkout).decode('utf-8').rstrip()
        revision = subprocess.check_output(['git', 'log', '-1', '--format', '%H', path], cwd=self._directory_in_checkout).decode('utf-8').rstrip()
        branch = subprocess.check_output(['git', 'symbolic-ref', '-q', 'HEAD'], cwd=self._directory_in_checkout).decode('utf-8').rstrip()
        branch = re.sub(r'^refs\/heads\/', '', branch) or 'master'
        return {branch, revision, repository_url}

    def svn_info_for_path(self, path):
        if self._last_svn_info and self._last_svn_info['path'] == path:
            # FIXME: We should also ensure that the checkout directory for the cached SVN info is
            # the same as the specified checkout directory.
            return self._last_svn_info

        svn_info_command = ['svn', 'info']
        if self._is_git_svn:
            svn_info_command = ['git'] + svn_info_command
        output = subprocess.check_output(svn_info_command + [path], cwd=self._directory_in_checkout).decode('utf-8').rstrip()
        if not output:
            return {}

        temp = {}
        lines = output.splitlines()
        for line in lines:
            key, value = line.split(': ', 1)
            if key and value:
                temp[key] = value

        svn_info = {
            'pathAsURL': temp['URL'],
            'repositoryRoot': temp['Repository Root'],
            'revision': temp['Revision'],
        }
        branch = svn_info['pathAsURL'].replace(svn_info['repositoryRoot'] + '/', '')
        branch = branch[0:branch.find('/')]
        svn_info['branch'] = branch

        # Although tempting to use temp['Path'] we cannot because it is relative to self._directory_in_checkout.
        # And self._directory_in_checkout may not be the top-level checkout directory. We need to compute the
        # relative path with respect to the top-level checkout directory.
        svn_info['path'] = svn_info['pathAsURL'].replace('{}/{}/'.format(svn_info['repositoryRoot'], branch), '')

        self._last_svn_info = svn_info

        return svn_info

    @staticmethod
    def permalink_for_path(path, line_number, revision_info, annotate_blame):
        revision = '&rev=' + str(revision_info['revision']) if 'revision' in revision_info else ''
        line_number = '#L' + str(line_number) if line_number else ''
        branch = revision_info.get('branch', 'trunk')
        annotate_blame = '&annotate=blame' if annotate_blame else ''
        return 'https://trac.webkit.org/browser/{}/{}?{}{}{}'.format(branch, path, revision, annotate_blame, line_number)

    @staticmethod
    def is_svn_directory(directory):
        try:
            subprocess.check_call(['svn', 'info'], cwd=directory)
        except:
            return False
        return True

    @staticmethod
    def is_git_directory(directory):
        try:
            subprocess.check_call(['git', 'rev-parse'], cwd=directory)
        except:
            return False
        return True

    @staticmethod
    def is_git_svn_directory(directory):
        try:
            return bool(subprocess.check_output(['git', 'config', '--get', 'svn-remote.svn.fetch'], cwd=directory, stderr=subprocess.STDOUT).decode('utf-8').rstrip())
        except:
            return False

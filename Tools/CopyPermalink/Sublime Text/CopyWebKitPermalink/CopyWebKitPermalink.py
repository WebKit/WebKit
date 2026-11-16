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
import urllib
import webbrowser

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
    def match_commit(self):
        return self._get('match_commit', default=True)

    @match_commit.setter
    def match_commit(self, value):
        self._set('match_commit', value)

    @property
    def automatically_open_in_browser(self):
        return self._get('automatically_open_in_browser', default=False)

    @automatically_open_in_browser.setter
    def automatically_open_in_browser(self, value):
        self._set('automatically_open_in_browser', value)

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
        self._directory_in_checkout = document_path if os.path.isdir(document_path) else os.path.dirname(document_path)

        if not self.is_git_directory():
            return

        repository_url = self.git_repository_url()

        mode = "blame" if annotate_blame else "blob"

        branch = self.current_git_commit() if s_settings.match_commit else self.current_git_branch()

        path = self.path_relative_to_repository_root_for_path(document_path)

        line_number, _ = self.view.rowcol(self.view.sel()[0].begin())  # Zero-based
        line_number = line_number + 1

        permalink = '{}/{}/{}/{}#L{}'.format(repository_url, mode, branch, urllib.parse.quote(path), line_number)
        print(permalink)
        sublime.set_clipboard(permalink)
        if s_settings.automatically_open_in_browser:
            webbrowser.open(permalink)

    def is_enabled(self):
        return len(self.view.sel()) > 0 and bool(self.view.file_name())

    def is_visible(self):
        return self.is_enabled()

    def description(self):
        return 'Copy WebKit Permalink'

    def is_git_directory(self):
        try:
            subprocess.check_call(['git', 'rev-parse'], cwd=self._directory_in_checkout)
        except:
            return False
        return True

    def git_repository_url(self):
        assert self.is_git_directory()

        repository_url = subprocess.check_output(['git', 'remote', 'get-url', 'origin'], cwd=self._directory_in_checkout).decode('utf-8').rstrip()
        repository_url = re.sub(r'^git@(.+?):(.+?)', r'https://\1/\2', repository_url)
        repository_url = re.sub(r'\.git$', '', repository_url)
        return repository_url

    def path_relative_to_repository_root_for_path(self, path):
        assert self.is_git_directory()

        return subprocess.check_output(['git', 'ls-tree', '--full-name', '--name-only', 'HEAD', path], cwd=self._directory_in_checkout).decode('utf-8').rstrip()

    def current_git_branch(self):
        assert self.is_git_directory()

        if not s_settings.match_commit:
            return 'main'

        branch = subprocess.check_output(['git', 'symbolic-ref', '-q', 'HEAD'], cwd=self._directory_in_checkout).decode('utf-8').rstrip()
        branch = re.sub(r'^refs\/heads\/', '', branch)
        return branch or 'main'

    def current_git_commit(self):
        assert self.is_git_directory()
        assert not s_settings.match_commit

        return subprocess.check_output(['git', 'log', '-1', '--format=%H'], cwd=self._directory_in_checkout).decode('utf-8').rstrip()

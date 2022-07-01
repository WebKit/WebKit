# Copyright (c) 2011 Google Inc. All rights reserved.
# Copyright (C) 2019 Apple Inc. All rights reserved.
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
#     * Neither the name of Google Inc. nor the names of its
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

import os
import platform
import sys

from webkitpy.common.system import environment, executive, filesystem, platforminfo, user, workspace
from webkitcorepy import FileLock


class SystemHost(object):
    _default_system_host = None

    def __init__(self):
        self.executive = executive.Executive()
        self.filesystem = filesystem.FileSystem()
        self.platform = platforminfo.PlatformInfo(executive=self.executive)
        self.user = user.User(self.platform)
        self.workspace = workspace.Workspace(self.filesystem, self.executive)

    def copy_current_environment(self):
        return environment.Environment(os.environ.copy())

    def make_file_lock(self, path):
        return FileLock(path)

    def symbolicate_crash_log_if_needed(self, path):
        return self.filesystem.read_text_file(path)

    def path_to_lldb_python_directory(self):
        if not self.platform.is_mac():
            return ''
        # Explicitly use Python 2.7
        path = self.executive.run_command(['xcrun', 'lldb', '--python-path'], return_stderr=False).rstrip()
        return self.filesystem.join(self.filesystem.dirname(path), 'Python')

    @property
    def device_type(self):
        return None

    @staticmethod
    def get_default():
        if not SystemHost._default_system_host:
            SystemHost._default_system_host = SystemHost()
        return SystemHost._default_system_host

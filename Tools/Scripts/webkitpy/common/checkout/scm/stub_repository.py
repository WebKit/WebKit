# Copyright (c) 2019 Apple Inc. All rights reserved.
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
#
# Python module for interacting with an SCM system (like SVN or Git)

import logging
import json

from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.checkout.scm.scm import SCM

_log = logging.getLogger(__name__)


class StubRepository(SCM):
    _stub_repository_json = 'checkout_information.json'

    def __init__(self, cwd, filesystem, **kwargs):
        self._filesystem = filesystem

    @classmethod
    def _find_parent_path_matching_callback_condition(cls, path, callback, filesystem=None):
        if not filesystem:
            filesystem = FileSystem()
        previous_path = ''
        path = filesystem.abspath(path)
        while path and path != previous_path:
            if filesystem.exists(filesystem.join(path, cls._stub_repository_json)):
                return callback(path)
            previous_path = path
            path = filesystem.dirname(path)
        return None

    @classmethod
    def in_working_directory(cls, path, executive=None, filesystem=None):
        try:
            return bool(cls._find_parent_path_matching_callback_condition(path, lambda path: True, filesystem=filesystem))
        except OSError:
            return False

    def svn_revision(self, path):
        return self.native_revision(path)

    def native_revision(self, path):
        return self._find_parent_path_matching_callback_condition(path, lambda path: self._decode_json(path)['id'], filesystem=self._filesystem)

    def native_branch(self, path):
        return self._find_parent_path_matching_callback_condition(path, lambda path: self._decode_json(path)['branch'], filesystem=self._filesystem)

    def _decode_json(self, path):
        with self._filesystem.open_text_file_for_reading(self._filesystem.join(path, self._stub_repository_json)) as json_file:
            return json.load(json_file)

    def find_checkout_root(self, path):
        return self._find_parent_path_matching_callback_condition(path, lambda path: path, filesystem=self._filesystem)

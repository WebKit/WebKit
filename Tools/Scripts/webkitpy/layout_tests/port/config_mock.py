#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
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

"""Wrapper objects for WebKit-specific utility routines."""

from webkitpy.common.system.filesystem_mock import MockFileSystem


class MockConfig(object):
    _FLAGS_FROM_CONFIGURATIONS = {
        "Debug": "--debug",
        "Release": "--release",
    }

    def __init__(self, filesystem=None, default_configuration='Release', port_implementation=None):
        self._filesystem = filesystem or MockFileSystem()
        self._default_configuration = default_configuration
        self._port_implmentation = port_implementation

    def flag_for_configuration(self, configuration):
        return self._FLAGS_FROM_CONFIGURATIONS[configuration]

    def build_directory(self, configuration):
        return "/mock-build"

    def build_dumprendertree(self, configuration):
        return True

    def default_configuration(self):
        return self._default_configuration

    def path_from_webkit_base(self, *comps):
        # FIXME: This could use self._filesystem.join, but that doesn't handle empty lists.
        return self.webkit_base_dir() + "/" + "/".join(list(comps))

    def script_path(self, script_name):
        # This is intentionally relative. Callers should pass the checkout_root/webkit_base_dir to run_command as the cwd.
        return self._filesystem.join("Tools", "Scripts", script_name)

    def webkit_base_dir(self):
        return "/mock-checkout"

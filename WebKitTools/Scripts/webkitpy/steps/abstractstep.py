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

from webkitpy.webkit_logging import log
from webkitpy.webkitport import WebKitPort


class AbstractStep(object):
    def __init__(self, tool, options):
        self._tool = tool
        self._options = options
        self._port = None

    def _run_script(self, script_name, quiet=False, port=WebKitPort):
        log("Running %s" % script_name)
        # FIXME: This should use self.port()
        self._tool.executive.run_and_throw_if_fail(port.script_path(script_name), quiet)

    # FIXME: The port should live on the tool.
    def port(self):
        if self._port:
            return self._port
        self._port = WebKitPort.port(self._options.port)
        return self._port

    _well_known_keys = {
        "diff" : lambda self: self._tool.scm().create_patch(),
        "changelogs" : lambda self: self._tool.scm().modified_changelogs(),
    }

    def cached_lookup(self, state, key, promise=None):
        if state.get(key):
            return state[key]
        if not promise:
            promise = self._well_known_keys.get(key)
        state[key] = promise(self)
        return state[key]

    @classmethod
    def options(cls):
        return []

    def run(self, state):
        raise NotImplementedError, "subclasses must implement"

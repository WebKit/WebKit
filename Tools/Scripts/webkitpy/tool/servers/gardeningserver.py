# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
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

import BaseHTTPServer
import os

from webkitpy.tool.servers.reflectionhandler import ReflectionHandler


class GardeningHTTPServer(BaseHTTPServer.HTTPServer):
    def __init__(self, httpd_port, config):
        server_name = ''
        self.tool = config['tool']
        BaseHTTPServer.HTTPServer.__init__(self, (server_name, httpd_port), GardeningHTTPRequestHandler)


class GardeningHTTPRequestHandler(ReflectionHandler):
    STATIC_FILE_NAMES = frozenset([
        "index.html",
        "base.js",
        "config.js",
        "main.js",
        "results.js",
        "ui.js",
        "favicon-green.png",
        "favicon-red.png",
    ])

    STATIC_FILE_DIRECTORY = os.path.join(os.path.dirname(__file__), "data", "gardeningserver")

    def _run_webkit_patch(self, args):
        return self.server.tool.executive.run_command([self.server.tool.path()] + args)

    def rollout(self):
        revision = self.query['revision'][0]
        reason = self.query['reason'][0]
        self._run_webkit_patch([
            'rollout',
            '--force-clean',
            '--non-interactive',
            revision,
            reason,
        ])
        self._serve_text('success')

    def rebaseline(self):
        builder = self.query['builder'][0]
        test = self.query['test'][0]
        suffix = self.query['suffix'][0]
        self._run_webkit_patch([
            'rebaseline-test',
            builder,
            test,
            suffix,
        ])
        self._serve_text('success')

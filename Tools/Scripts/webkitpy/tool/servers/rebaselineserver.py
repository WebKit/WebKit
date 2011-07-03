# Copyright (c) 2010 Google Inc. All rights reserved.
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

from __future__ import with_statement

from __future__ import with_statement

import codecs
import datetime
import fnmatch
import mimetypes
import os
import os.path
import shutil
import threading
import time
import urlparse
import BaseHTTPServer

from optparse import make_option
from wsgiref.handlers import format_date_time

from webkitpy.common import system
from webkitpy.common.net import resultsjsonparser
from webkitpy.layout_tests.layout_package import json_results_generator
from webkitpy.layout_tests.port import factory
from webkitpy.layout_tests.port.webkit import WebKitPort
from webkitpy.tool.multicommandtool import AbstractDeclarativeCommand
from webkitpy.thirdparty import simplejson

STATE_NEEDS_REBASELINE = 'needs_rebaseline'
STATE_REBASELINE_FAILED = 'rebaseline_failed'
STATE_REBASELINE_SUCCEEDED = 'rebaseline_succeeded'


class RebaselineHTTPServer(BaseHTTPServer.HTTPServer):
    def __init__(self, httpd_port, test_config, results_json, platforms_json):
        BaseHTTPServer.HTTPServer.__init__(self, ("", httpd_port), RebaselineHTTPRequestHandler)
        self.test_config = test_config
        self.results_json = results_json
        self.platforms_json = platforms_json


class RebaselineHTTPRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
    STATIC_FILE_NAMES = frozenset([
        "index.html",
        "loupe.js",
        "main.js",
        "main.css",
        "queue.js",
        "util.js",
    ])

    STATIC_FILE_DIRECTORY = os.path.join(
        os.path.dirname(__file__), "data", "rebaselineserver")

    def do_GET(self):
        self._handle_request()

    def do_POST(self):
        self._handle_request()

    def _handle_request(self):
        # Parse input.
        if "?" in self.path:
            path, query_string = self.path.split("?", 1)
            self.query = urlparse.parse_qs(query_string)
        else:
            path = self.path
            self.query = {}
        function_or_file_name = path[1:] or "index.html"

        # See if a static file matches.
        if function_or_file_name in RebaselineHTTPRequestHandler.STATIC_FILE_NAMES:
            self._serve_static_file(function_or_file_name)
            return

        # See if a class method matches.
        function_name = function_or_file_name.replace(".", "_")
        if not hasattr(self, function_name):
            self.send_error(404, "Unknown function %s" % function_name)
            return
        if function_name[0] == "_":
            self.send_error(
                401, "Not allowed to invoke private or protected methods")
            return
        function = getattr(self, function_name)
        function()

    def _serve_static_file(self, static_path):
        self._serve_file(os.path.join(
            RebaselineHTTPRequestHandler.STATIC_FILE_DIRECTORY, static_path))

    def rebaseline(self):
        test = self.query['test'][0]
        baseline_target = self.query['baseline-target'][0]
        baseline_move_to = self.query['baseline-move-to'][0]
        test_json = self.server.results_json['tests'][test]

        if test_json['state'] != STATE_NEEDS_REBASELINE:
            self.send_error(400, "Test %s is in unexpected state: %s" %
                (test, test_json["state"]))
            return

        log = []
        success = _rebaseline_test(
            test,
            baseline_target,
            baseline_move_to,
            self.server.test_config,
            log=lambda l: log.append(l))

        if success:
            test_json['state'] = STATE_REBASELINE_SUCCEEDED
            self.send_response(200)
        else:
            test_json['state'] = STATE_REBASELINE_FAILED
            self.send_response(500)

        self.send_header('Content-type', 'text/plain')
        self.end_headers()
        self.wfile.write('\n'.join(log))

    def quitquitquit(self):
        self.send_response(200)
        self.send_header("Content-type", "text/plain")
        self.end_headers()
        self.wfile.write("Quit.\n")

        # Shutdown has to happen on another thread from the server's thread,
        # otherwise there's a deadlock
        threading.Thread(target=lambda: self.server.shutdown()).start()

    def test_result(self):
        test_name, _ = os.path.splitext(self.query['test'][0])
        mode = self.query['mode'][0]
        if mode == 'expected-image':
            file_name = test_name + '-expected.png'
        elif mode == 'actual-image':
            file_name = test_name + '-actual.png'
        if mode == 'expected-checksum':
            file_name = test_name + '-expected.checksum'
        elif mode == 'actual-checksum':
            file_name = test_name + '-actual.checksum'
        elif mode == 'diff-image':
            file_name = test_name + '-diff.png'
        if mode == 'expected-text':
            file_name = test_name + '-expected.txt'
        elif mode == 'actual-text':
            file_name = test_name + '-actual.txt'
        elif mode == 'diff-text':
            file_name = test_name + '-diff.txt'
        elif mode == 'diff-text-pretty':
            file_name = test_name + '-pretty-diff.html'

        file_path = os.path.join(self.server.test_config.results_directory, file_name)

        # Let results be cached for 60 seconds, so that they can be pre-fetched
        # by the UI
        self._serve_file(file_path, cacheable_seconds=60)

    def results_json(self):
        self._serve_json(self.server.results_json)

    def platforms_json(self):
        self._serve_json(self.server.platforms_json)

    def _serve_json(self, json):
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.end_headers()
        simplejson.dump(json, self.wfile)

    def _serve_file(self, file_path, cacheable_seconds=0):
        if not os.path.exists(file_path):
            self.send_error(404, "File not found")
            return
        with codecs.open(file_path, "rb") as static_file:
            self.send_response(200)
            self.send_header("Content-Length", os.path.getsize(file_path))
            mime_type, encoding = mimetypes.guess_type(file_path)
            if mime_type:
                self.send_header("Content-type", mime_type)

            if cacheable_seconds:
                expires_time = (datetime.datetime.now() +
                    datetime.timedelta(0, cacheable_seconds))
                expires_formatted = format_date_time(
                    time.mktime(expires_time.timetuple()))
                self.send_header("Expires", expires_formatted)
            self.end_headers()

            shutil.copyfileobj(static_file, self.wfile)

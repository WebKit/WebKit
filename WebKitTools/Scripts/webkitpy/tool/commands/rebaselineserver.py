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

"""Starts a local HTTP server which displays layout test failures (given a test
results directory), provides comparisons of expected and actual results (both
images and text) and allows one-click rebaselining of tests."""
from __future__ import with_statement

import codecs
import datetime
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

from webkitpy.tool.multicommandtool import AbstractDeclarativeCommand
import webkitpy.thirdparty.simplejson as simplejson

STATE_NEEDS_REBASELINE = 'needs_rebaseline'
STATE_REBASELINE_FAILED = 'rebaseline_failed'
STATE_REBASELINE_SUCCEEDED = 'rebaseline_succeeded'

class RebaselineHTTPServer(BaseHTTPServer.HTTPServer):
    def __init__(self, httpd_port, results_directory, results_json):
        BaseHTTPServer.HTTPServer.__init__(self, ("", httpd_port), RebaselineHTTPRequestHandler)
        self.results_directory = results_directory
        self.results_json = results_json


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

        file_path = os.path.join(self.server.results_directory, file_name)

        # Let results be cached for 60 seconds, so that they can be pre-fetched
        # by the UI
        self._serve_file(file_path, cacheable_seconds=60)

    def results_json(self):
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.end_headers()
        simplejson.dump(self.server.results_json, self.wfile)

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


class RebaselineServer(AbstractDeclarativeCommand):
    name = "rebaseline-server"
    help_text = __doc__
    argument_names = "/path/to/results/directory"

    def __init__(self):
        options = [
            make_option("--httpd-port", action="store", type="int", default=8127, help="Port to use for the the rebaseline HTTP server"),
        ]
        AbstractDeclarativeCommand.__init__(self, options=options)

    def execute(self, options, args, tool):
        results_directory = args[0]

        print 'Parsing unexpected_results.json...'
        results_json_path = os.path.join(
            results_directory, 'unexpected_results.json')
        with codecs.open(results_json_path, "r") as results_json_file:
            results_json_file = file(results_json_path)
            results_json = simplejson.load(results_json_file)

        for test_file, test_json in results_json['tests'].items():
            test_json['state'] = STATE_NEEDS_REBASELINE

        print "Starting server at http://localhost:%d/" % options.httpd_port
        print ("Use the 'Exit' link in the UI, http://localhost:%d/"
            "quitquitquit or Ctrl-C to stop") % options.httpd_port

        httpd = RebaselineHTTPServer(
            httpd_port=options.httpd_port,
            results_directory=results_directory,
            results_json=results_json)
        httpd.serve_forever()

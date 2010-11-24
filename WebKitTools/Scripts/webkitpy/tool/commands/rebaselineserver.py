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

from webkitpy.common import system
from webkitpy.layout_tests.port import factory
from webkitpy.layout_tests.port.webkit import WebKitPort
from webkitpy.tool.multicommandtool import AbstractDeclarativeCommand
from webkitpy.thirdparty import simplejson

STATE_NEEDS_REBASELINE = 'needs_rebaseline'
STATE_REBASELINE_FAILED = 'rebaseline_failed'
STATE_REBASELINE_SUCCEEDED = 'rebaseline_succeeded'

class RebaselineHTTPServer(BaseHTTPServer.HTTPServer):
    def __init__(self, httpd_port, results_directory, results_json, platforms_json):
        BaseHTTPServer.HTTPServer.__init__(self, ("", httpd_port), RebaselineHTTPRequestHandler)
        self.results_directory = results_directory
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


def _get_test_baselines(test_file, layout_tests_directory, platforms, filesystem):
    class AllPlatformsPort(WebKitPort):
        def __init__(self):
            WebKitPort.__init__(self, filesystem=filesystem)
            self._platforms_by_directory = dict(
                [(self._webkit_baseline_path(p), p) for p in platforms])

        def baseline_search_path(self):
            return self._platforms_by_directory.keys()

        def platform_from_directory(self, directory):
            return self._platforms_by_directory[directory]

    all_platforms_port = AllPlatformsPort()

    test_baselines = {}
    for baseline_extension in ('.txt', '.checksum', '.png'):
        test_path = filesystem.join(layout_tests_directory, test_file)
        baselines = all_platforms_port.expected_baselines(
            test_path, baseline_extension, all_baselines=True)
        for platform_directory, expected_filename in baselines:
            if not platform_directory:
                continue
            if platform_directory == layout_tests_directory:
                platform = 'base'
            else:
                platform = all_platforms_port.platform_from_directory(
                    platform_directory)
            if platform not in test_baselines:
                test_baselines[platform] = []
            test_baselines[platform].append(baseline_extension)
        
    for platform, extensions in test_baselines.items():
        test_baselines[platform] = tuple(extensions)

    return test_baselines

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
        filesystem = system.filesystem.FileSystem()

        print 'Parsing unexpected_results.json...'
        results_json_path = filesystem.join(
            results_directory, 'unexpected_results.json')
        with codecs.open(results_json_path, "r") as results_json_file:
            results_json_file = file(results_json_path)
            results_json = simplejson.load(results_json_file)

        port = factory.get()
        layout_tests_directory = port.layout_tests_dir()
        platforms = filesystem.listdir(
            filesystem.join(layout_tests_directory, 'platform'))

        print 'Gathering current baselines...'
        for test_file, test_json in results_json['tests'].items():
            test_json['state'] = STATE_NEEDS_REBASELINE
            test_json['baselines'] = _get_test_baselines(
                test_file, layout_tests_directory, platforms, filesystem)

        print "Starting server at http://localhost:%d/" % options.httpd_port
        print ("Use the 'Exit' link in the UI, http://localhost:%d/"
            "quitquitquit or Ctrl-C to stop") % options.httpd_port

        httpd = RebaselineHTTPServer(
            httpd_port=options.httpd_port,
            results_directory=results_directory,
            results_json=results_json,
            platforms_json={
                'platforms': platforms,
                'defaultPlatform': port.name(),
            })
        httpd.serve_forever()

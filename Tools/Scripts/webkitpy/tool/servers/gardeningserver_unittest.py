# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

try:
    import json
except ImportError:
    # python 2.5 compatibility
    import webkitpy.thirdparty.simplejson as json

import unittest

from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.mocktool import MockTool, MockExecutive
from webkitpy.tool.servers.gardeningserver import *


class MockServer(object):
    def __init__(self):
        self.tool = MockTool()
        self.tool.executive = MockExecutive(should_log=True)


# The real GardeningHTTPRequestHandler has a constructor that's too hard to
# call in a unit test, so we create a subclass that's easier to constrcut.
class TestGardeningHTTPRequestHandler(GardeningHTTPRequestHandler):
    def __init__(self, server):
        self.server = server

    def _serve_text(self, text):
        print "== Begin Response =="
        print text
        print "== End Response =="

    def _serve_json(self, json_object):
        print "== Begin JSON Response =="
        print json.dumps(json_object)
        print "== End JSON Response =="


class GardeningServerTest(unittest.TestCase):
    def _post_to_path(self, path, expected_stderr=None, expected_stdout=None):
        handler = TestGardeningHTTPRequestHandler(MockServer())
        handler.path = path
        OutputCapture().assert_outputs(self, handler.do_POST, expected_stderr=expected_stderr, expected_stdout=expected_stdout)

    def test_changelog(self):
        expected_stderr = "MOCK run_and_throw_if_fail: ['mock-update-webkit'], cwd=/mock-checkout\n"
        expected_stdout = """== Begin JSON Response ==
{"bug_id": 42, "author_email": "abarth@webkit.org", "reviewer_text": "Darin Adler", "author_name": "Adam Barth", "changed_files": ["path/to/file", "another/file"]}
== End JSON Response ==
"""
        self._post_to_path("/changelog?revision=2314", expected_stderr=expected_stderr, expected_stdout=expected_stdout)

    def test_buildbot(self):
        expected_stdout = '== Begin JSON Response ==\n[{"is_green": true, "name": "Builder1", "activity": "building"}, {"is_green": true, "name": "Builder2", "activity": "idle"}]\n== End JSON Response ==\n'
        self._post_to_path("/buildbot", expected_stdout=expected_stdout, expected_stderr='')

    def test_rollout(self):
        expected_stderr = "MOCK run_command: ['echo', 'rollout', '--force-clean', '--non-interactive', '2314', 'MOCK rollout reason'], cwd=/mock-checkout\n"
        expected_stdout = "== Begin Response ==\nsuccess\n== End Response ==\n"
        self._post_to_path("/rollout?revision=2314&reason=MOCK+rollout+reason", expected_stderr=expected_stderr, expected_stdout=expected_stdout)

    def test_rebaseline(self):
        expected_stderr = "MOCK run_command: ['echo', 'rebaseline-test', 'MOCK builder', 'user-scripts/another-test.html', 'txt'], cwd=/mock-checkout\n"
        expected_stdout = "== Begin Response ==\nsuccess\n== End Response ==\n"
        self._post_to_path("/rebaseline?builder=MOCK+builder&test=user-scripts/another-test.html&suffix=txt", expected_stderr=expected_stderr, expected_stdout=expected_stdout)

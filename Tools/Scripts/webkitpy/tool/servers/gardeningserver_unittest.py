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

import json
import unittest

from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.layout_tests.models.test_configuration import *
from webkitpy.layout_tests.port import builders
from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.mocktool import MockTool
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.host_mock import MockHost
from webkitpy.tool.servers.gardeningserver import *


class TestPortFactory(object):
    # FIXME: Why is this a class method?
    @classmethod
    def create(cls):
        host = MockHost()
        return host.port_factory.get("test-win-xp")

    @classmethod
    def path_to_test_expectations_file(cls):
        return cls.create().path_to_test_expectations_file()


class MockServer(object):
    def __init__(self):
        self.tool = MockTool()
        self.tool.executive = MockExecutive(should_log=True)
        self.tool.filesystem.files[TestPortFactory.path_to_test_expectations_file()] = ""


# The real GardeningHTTPRequestHandler has a constructor that's too hard to
# call in a unit test, so we create a subclass that's easier to constrcut.
class TestGardeningHTTPRequestHandler(GardeningHTTPRequestHandler):
    def __init__(self, server):
        self.server = server
        self.body = None

    def _expectations_updater(self):
        return GardeningExpectationsUpdater(self.server.tool, TestPortFactory.create())

    def _read_entity_body(self):
        return self.body if self.body else ''

    def _serve_text(self, text):
        print "== Begin Response =="
        print text
        print "== End Response =="

    def _serve_json(self, json_object):
        print "== Begin JSON Response =="
        print json.dumps(json_object)
        print "== End JSON Response =="


class BuildCoverageExtrapolatorTest(unittest.TestCase):
    def test_extrapolate(self):
        # FIXME: Make this test not rely on actual (not mock) port objects.
        host = MockHost()
        port = host.port_factory.get('chromium-win-win7', None)
        converter = TestConfigurationConverter(port.all_test_configurations(), port.configuration_specifier_macros())
        extrapolator = BuildCoverageExtrapolator(converter)
        self.assertEquals(extrapolator.extrapolate_test_configurations("Webkit Win"), set([TestConfiguration(version='xp', architecture='x86', build_type='release')]))
        self.assertEquals(extrapolator.extrapolate_test_configurations("Webkit Vista"), set([
            TestConfiguration(version='vista', architecture='x86', build_type='debug'),
            TestConfiguration(version='vista', architecture='x86', build_type='release')]))
        self.assertRaises(KeyError, extrapolator.extrapolate_test_configurations, "Potato")


class GardeningExpectationsUpdaterTest(unittest.TestCase):
    def __init__(self, testFunc):
        self.tool = MockTool()
        self.tool.executive = MockExecutive(should_log=True)
        self.tool.filesystem.files[TestPortFactory.path_to_test_expectations_file()] = ""
        unittest.TestCase.__init__(self, testFunc)

    def assert_update(self, failure_info_list, expectations_before=None, expectations_after=None, expected_exception=None):
        updater = GardeningExpectationsUpdater(self.tool, TestPortFactory.create())
        path_to_test_expectations_file = TestPortFactory.path_to_test_expectations_file()
        self.tool.filesystem.files[path_to_test_expectations_file] = expectations_before or ""
        if expected_exception:
            self.assertRaises(expected_exception, updater.update_expectations, (failure_info_list))
        else:
            updater.update_expectations(failure_info_list)
            self.assertEquals(self.tool.filesystem.files[path_to_test_expectations_file], expectations_after)

    def test_empty_expectations(self):
        failure_info_list = []
        expectations_before = ""
        expectations_after = ""
        self.assert_update(failure_info_list, expectations_before=expectations_before, expectations_after=expectations_after)

    def test_unknown_builder(self):
        failure_info_list = [{"testName": "failures/expected/image.html", "builderName": "Bob", "failureTypeList": ["IMAGE"]}]
        self.assert_update(failure_info_list, expected_exception=KeyError)

    def test_empty_failure_type_list(self):
        failure_info_list = [{"testName": "failures/expected/image.html", "builderName": "Webkit Win", "failureTypeList": []}]
        self.assert_update(failure_info_list, expected_exception=AssertionError)

    def test_empty_test_name(self):
        failure_info_list = [{"testName": "", "builderName": "Webkit Win", "failureTypeList": ["TEXT"]}]
        self.assert_update(failure_info_list, expected_exception=AssertionError)

    def test_unknown_failure_type(self):
        failure_info_list = [{"testName": "failures/expected/image.html", "builderName": "Webkit Win", "failureTypeList": ["IMAGE", "EXPLODE"]}]
        expectations_before = ""
        expectations_after = "\nBUG_NEW XP RELEASE : failures/expected/image.html = IMAGE"
        self.assert_update(failure_info_list, expectations_before=expectations_before, expectations_after=expectations_after)

    def test_add_new_expectation(self):
        failure_info_list = [{"testName": "failures/expected/image.html", "builderName": "Webkit Win", "failureTypeList": ["IMAGE"]}]
        expectations_before = ""
        expectations_after = "\nBUG_NEW XP RELEASE : failures/expected/image.html = IMAGE"
        self.assert_update(failure_info_list, expectations_before=expectations_before, expectations_after=expectations_after)

    def test_replace_old_expectation(self):
        failure_info_list = [{"testName": "failures/expected/image.html", "builderName": "Webkit Win", "failureTypeList": ["IMAGE"]}]
        expectations_before = "BUG_OLD XP RELEASE : failures/expected/image.html = TEXT"
        expectations_after = "BUG_NEW XP RELEASE : failures/expected/image.html = IMAGE"
        self.assert_update(failure_info_list, expectations_before=expectations_before, expectations_after=expectations_after)

    def test_pass_expectation(self):
        failure_info_list = [{"testName": "failures/expected/image.html", "builderName": "Webkit Win", "failureTypeList": ["PASS"]}]
        expectations_before = "BUG_OLD XP RELEASE : failures/expected/image.html = TEXT"
        expectations_after = ""
        self.assert_update(failure_info_list, expectations_before=expectations_before, expectations_after=expectations_after)

    def test_supplement_old_expectation(self):
        failure_info_list = [{"testName": "failures/expected/image.html", "builderName": "Webkit Win", "failureTypeList": ["IMAGE"]}]
        expectations_before = "BUG_OLD XP RELEASE :  failures/expected/text.html = TEXT"
        expectations_after = ("BUG_OLD XP RELEASE :  failures/expected/text.html = TEXT\n"
                              "BUG_NEW XP RELEASE : failures/expected/image.html = IMAGE")
        self.assert_update(failure_info_list, expectations_before=expectations_before, expectations_after=expectations_after)

    def test_spurious_updates(self):
        failure_info_list = [{"testName": "failures/expected/image.html", "builderName": "Webkit Win", "failureTypeList": ["IMAGE"]}]
        expectations_before = "BUG_OLDER MAC LINUX : failures/expected/image.html = IMAGE+TEXT\nBUG_OLD XP RELEASE :  failures/expected/image.html = TEXT"
        expectations_after = "BUG_OLDER MAC LINUX : failures/expected/image.html = IMAGE+TEXT\nBUG_NEW XP RELEASE : failures/expected/image.html = IMAGE"
        self.assert_update(failure_info_list, expectations_before=expectations_before, expectations_after=expectations_after)


class GardeningServerTest(unittest.TestCase):
    def _post_to_path(self, path, body=None, expected_stderr=None, expected_stdout=None):
        handler = TestGardeningHTTPRequestHandler(MockServer())
        handler.path = path
        handler.body = body
        OutputCapture().assert_outputs(self, handler.do_POST, expected_stderr=expected_stderr, expected_stdout=expected_stdout)

    def test_rollout(self):
        expected_stderr = "MOCK run_command: ['echo', 'rollout', '--force-clean', '--non-interactive', '2314', 'MOCK rollout reason'], cwd=/mock-checkout\n"
        expected_stdout = "== Begin Response ==\nsuccess\n== End Response ==\n"
        self._post_to_path("/rollout?revision=2314&reason=MOCK+rollout+reason", expected_stderr=expected_stderr, expected_stdout=expected_stdout)

    def test_rebaseline(self):
        builders._exact_matches = {"MOCK builder": {"port_name": "mock-port-name", "specifiers": set(["mock-specifier"])}}
        expected_stderr = "MOCK run_command: ['echo', 'rebaseline-test', '--suffixes', 'txt,png', 'MOCK builder', 'user-scripts/another-test.html'], cwd=/mock-checkout\n"
        expected_stdout = "== Begin Response ==\nsuccess\n== End Response ==\n"
        self._post_to_path("/rebaseline?builder=MOCK+builder&test=user-scripts/another-test.html&suffixes=txt,png", expected_stderr=expected_stderr, expected_stdout=expected_stdout)

    def test_rebaseline_new_port(self):
        builders._exact_matches = {"MOCK builder": {"port_name": "mock-port-name", "specifiers": set(["mock-specifier"]), "move_overwritten_baselines_to": ["mock-port-fallback", "mock-port-fallback2"]}}
        expected_stderr = "MOCK run_command: ['echo', 'rebaseline-test', '--suffixes', 'txt,png', 'MOCK builder', 'user-scripts/another-test.html', 'mock-port-fallback', 'mock-port-fallback2'], cwd=/mock-checkout\n"
        expected_stdout = "== Begin Response ==\nsuccess\n== End Response ==\n"
        self._post_to_path("/rebaseline?builder=MOCK+builder&test=user-scripts/another-test.html&suffixes=txt,png", expected_stderr=expected_stderr, expected_stdout=expected_stdout)

    def test_optimizebaselines(self):
        expected_stderr = "MOCK run_command: ['echo', 'optimize-baselines', '--suffixes', 'txt,png', 'user-scripts/another-test.html'], cwd=/mock-checkout\n"
        expected_stdout = "== Begin Response ==\nsuccess\n== End Response ==\n"
        self._post_to_path("/optimizebaselines?test=user-scripts/another-test.html&suffixes=txt,png", expected_stderr=expected_stderr, expected_stdout=expected_stdout)

    def test_updateexpectations(self):
        expected_stderr = ""
        expected_stdout = "== Begin Response ==\nsuccess\n== End Response ==\n"
        self._post_to_path("/updateexpectations", body="[]", expected_stderr=expected_stderr, expected_stdout=expected_stdout)

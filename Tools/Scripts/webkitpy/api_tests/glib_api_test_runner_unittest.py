# Copyright (C) 2026 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

"""Unit tests for Tools/glib/api_test_runner.py."""

import optparse
import os
import sys
import tempfile
import unittest

tools_directory = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
sys.path.insert(0, os.path.join(tools_directory, 'glib'))
sys.path.insert(0, os.path.join(tools_directory, 'jhbuild'))

from api_test_runner import TestRunner  # noqa: E402


class StubTestRunner(TestRunner):
    def __init__(self, base_directory, tests=None):
        self._base_directory = base_directory
        self._initial_test_list = tests or []

    def _test_programs_base_dir(self):
        return self._base_directory

    def is_google_test(self, test_program):
        return os.path.basename(test_program) == "TestWTF"


class StubExpectations(object):
    def __init__(self, expectations):
        self._expectations = expectations

    def get_expectation(self, test, subtest):
        return self._expectations.get(subtest, ['PASS'])


class RepeatingStubTestRunner(StubTestRunner):
    def __init__(self, base_directory, runs, expectations={}):
        super().__init__(base_directory)
        self._options = optparse.Values({'repeat_each': len(runs)})
        self._expectations = StubExpectations(expectations)
        self._runs = iter(runs)
        self.requested_subtests = []

    def _run_test(self, test_program, subtests, skipped_test_cases):
        self.requested_subtests.append(subtests)
        return next(self._runs)


class GLibAPITestRunnerTest(unittest.TestCase):
    def setUp(self):
        self._temporary_directory = tempfile.TemporaryDirectory()
        self._base_directory = self._temporary_directory.name
        self._wpe_directory = os.path.join(self._base_directory, "WPE")
        self._gtk_directory = os.path.join(self._base_directory, "WebKitGTK")
        os.mkdir(self._wpe_directory)
        os.mkdir(self._gtk_directory)
        self._glib_test = self._create_executable(self._wpe_directory, "TestDownloads")
        self._gtk_test = self._create_executable(self._gtk_directory, "TestWebKitWebView")
        self._google_test = self._create_executable(self._base_directory, "TestWTF")

    def tearDown(self):
        self._temporary_directory.cleanup()

    def _create_executable(self, directory, name):
        path = os.path.join(directory, name)
        with open(path, "w"):
            pass
        os.chmod(path, 0o755)
        return path

    def test_resolves_bare_and_prefixed_binary_names(self):
        runner = StubTestRunner(self._base_directory)

        self.assertEqual(self._glib_test, runner._resolve_test_program("TestDownloads"))
        self.assertEqual(self._glib_test, runner._resolve_test_program("WPE/TestDownloads"))
        self.assertEqual(self._gtk_test, runner._resolve_test_program("TestWebKitWebView"))
        self.assertEqual(self._gtk_test, runner._resolve_test_program("WebKitGTK/TestWebKitWebView"))
        self.assertEqual(self._google_test, runner._resolve_test_program("TestWTF"))

    def test_canonical_and_prefixed_names_select_the_same_glib_subtest(self):
        subtest = "/webkit/Downloads/download"
        for requested_name in (
            f"TestDownloads:{subtest}",
            f"WPE/TestDownloads:{subtest}",
        ):
            with self.subTest(requested_name=requested_name):
                runner = StubTestRunner(self._base_directory, [requested_name])
                self.assertEqual([self._glib_test], runner._get_tests([requested_name]))
                self.assertEqual([subtest], runner._getsubtests_to_run_for_test(self._glib_test))

    def test_canonical_google_test_name_resolves_and_selects_only_its_test_case(self):
        requested_name = "TestWTF.WTF_GUniquePtr.Basic"
        runner = StubTestRunner(self._base_directory, [requested_name])

        self.assertEqual([self._google_test], runner._get_tests([requested_name]))
        self.assertEqual(["WTF_GUniquePtr.Basic"], runner._getsubtests_to_run_for_test(self._google_test))

    def test_multiple_subtests_are_preserved(self):
        requested_names = [
            "TestDownloads:/webkit/Downloads/first",
            "TestDownloads:/webkit/Downloads/second",
        ]
        runner = StubTestRunner(self._base_directory, requested_names)

        self.assertEqual([self._glib_test], runner._get_tests(requested_names))
        self.assertEqual([
            "/webkit/Downloads/first",
            "/webkit/Downloads/second",
        ], runner._getsubtests_to_run_for_test(self._glib_test))

    def test_uploaded_names_can_be_passed_back_to_the_runner(self):
        test_cases = (
            (self._glib_test, "/webkit/Downloads/download", "TestDownloads:/webkit/Downloads/download"),
            (self._gtk_test, "/webkit/WebKitWebView/title", "TestWebKitWebView:/webkit/WebKitWebView/title"),
            (self._google_test, "WTF_GUniquePtr.Basic", "TestWTF.WTF_GUniquePtr.Basic"),
        )
        for test_program, test_case, expected_name in test_cases:
            with self.subTest(test_program=test_program):
                runner = StubTestRunner(self._base_directory)
                uploaded_name = runner._get_test_name_for_upload(test_program, test_case)
                runner._initial_test_list = [uploaded_name]

                self.assertEqual(expected_name, uploaded_name)
                self.assertEqual([test_program], runner._get_tests([uploaded_name]))
                self.assertEqual([test_case], runner._getsubtests_to_run_for_test(test_program))

    def test_json_output_uses_uploaded_test_names(self):
        runner = StubTestRunner(self._base_directory)

        self.assertEqual([
            {"name": "TestDownloads:/webkit/Downloads/download", "output": None},
            {"name": "TestWebKitWebView:/webkit/WebKitWebView/title", "output": None},
            {"name": "TestWTF.WTF_GUniquePtr.Basic", "output": None},
        ], runner._generate_test_list_for_json_output({
            self._glib_test: ["/webkit/Downloads/download"],
            self._gtk_test: ["/webkit/WebKitWebView/title"],
            self._google_test: ["WTF_GUniquePtr.Basic"],
        }))

    def test_results_use_uploaded_test_names_and_omit_actual_for_passes(self):
        runner = StubTestRunner(self._base_directory)

        self.assertEqual({
            "TestDownloads:/webkit/Downloads/download": {"actual": "FAIL", "expected": "PASS"},
            "TestDownloads:/webkit/Downloads/flaky": {"actual": "TIMEOUT", "expected": "PASS TIMEOUT"},
            "TestWebKitWebView:/webkit/WebKitWebView/title": {"expected": "PASS"},
            "TestWTF.WTF_GUniquePtr.Basic": {"actual": "CRASH"},
        }, runner._results_for_upload({
            self._glib_test: [
                ("/webkit/Downloads/download", "FAIL", ["PASS"]),
                ("/webkit/Downloads/flaky", "TIMEOUT", ["PASS", "TIMEOUT"]),
            ],
            self._gtk_test: [("/webkit/WebKitWebView/title", None, ["PASS"])],
            self._google_test: [("WTF_GUniquePtr.Basic", "CRASH", [])],
        }))

    def test_repeat_each_runs_the_requested_tests_every_time(self):
        runner = RepeatingStubTestRunner(self._base_directory, [{"/first": "PASS"}] * 3)

        self.assertEqual({"/first": "PASS"}, runner._run_test_repeatedly(self._glib_test, ["/first"], []))
        self.assertEqual([["/first"]] * 3, runner.requested_subtests)

    def test_repeat_each_keeps_the_first_unexpected_result(self):
        runner = RepeatingStubTestRunner(self._base_directory, [
            {"/first": "PASS", "/second": "PASS", "/third": "FAIL"},
            {"/first": "FAIL", "/second": "PASS", "/third": "FAIL"},
            {"/first": "CRASH", "/second": "TIMEOUT", "/third": "PASS"},
        ], expectations={"/third": ["FAIL"]})

        self.assertEqual({"/first": "FAIL", "/second": "TIMEOUT", "/third": "PASS"},
                         runner._run_test_repeatedly(self._glib_test, [], []))


if __name__ == "__main__":
    unittest.main()

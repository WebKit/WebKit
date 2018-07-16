# Copyright (C) 2018 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest

from webkitpy.common.config.ports_mock import MockPort
from webkitpy.common.host_mock import MockHost
from webkitpy.w3c.wpt_runner import WPTRunner, parse_args


TEST_EXPECTATIONS_JSON_CONTENT = """{
    "passing_test.html": { "expected": "PASS" },
    "failing_test.html": { "expected": "FAIL" },
    "disabled_test.html": { "disabled": "Test Name #1" },
    "custom_test_name.html": {
        "test_name": "Custom Test Name",
        "expected": "FAIL"
    },
    "test_with_subtests.html": {
        "subtests": {
            "Subtest #1": { "expected": "PASS" },
            "Subtest #2": { "expected": "TIMEOUT" }
        }
    },
    "nested/test_file.html": { "expected": "PASS" },
    "nested/nested/test_file.html": {
        "test_name": "Deeper-nested test case",
        "subtests": {
            "Only Subtest": { "expected": "PASS" }
        }
    }
}"""

EXPECTED_TEST_MANIFESTS = {
    "passing_test.html.ini":
"""[passing_test.html]
    expected: PASS
""",

    "failing_test.html.ini":
"""[failing_test.html]
    expected: FAIL
""",

    "disabled_test.html.ini":
"""[disabled_test.html]
    disabled: Test Name #1
""",

    "custom_test_name.html.ini":
"""[Custom Test Name]
    expected: FAIL
""",

    "test_with_subtests.html.ini":
"""[test_with_subtests.html]
    [Subtest #1]
        expected: PASS
    [Subtest #2]
        expected: TIMEOUT
""",

    "nested/test_file.html.ini":
"""[test_file.html]
    expected: PASS
""",

    "nested/nested/test_file.html.ini":
"""[Deeper-nested test case]
    [Only Subtest]
        expected: PASS
"""
}


class WPTRunnerTest(unittest.TestCase):

    class MockTestDownloader(object):
        @staticmethod
        def default_options():
            return {}

        def __init__(self, repository_directory, host, options):
            self._repository_directory = repository_directory
            self._host = host

        def clone_tests(self):
            self._host.filesystem.maybe_make_directory(self._repository_directory, "web-platform-tests")

    class MockWebDriver(object):
        @staticmethod
        def create(port):
            return WPTRunnerTest.MockWebDriver()

        def binary_path(self):
            return "/mock-webdriver/bin/webdriver"

        def browser_path(self):
            return "/mock-webdriver/bin/browser"

        def browser_args(self):
            return ["webdriver_arg1", "webdriver_arg2"]

    class MockSpawnWPT(object):
        def __init__(self, test_case, expected_wpt_checkout=None, expected_wpt_args=None):
            self._test_case = test_case
            self._expected_wpt_checkout = expected_wpt_checkout
            self._expected_wpt_args = expected_wpt_args

        def __call__(self, script_name, wpt_checkout, wpt_args):
            self._test_case.assertEquals(script_name, "wptrunner_unittest")
            self._test_case.assertEquals(wpt_checkout, self._expected_wpt_checkout)
            self._test_case.assertEquals(wpt_args, self._expected_wpt_args)

    class TestInstance(object):
        def __init__(self, options, spawn_wpt_func=None):
            self.port = MockPort()
            self.host = MockHost()

            # In non-test environments, this value is by default set to the WEBKIT_TEST_CHILD_PROCESSES
            # env value or, if that's not present, to the default number of child processes. Here we
            # manually set it to 4 for consistency, unless the test case specifies it.
            if not options.child_processes:
                options.child_processes = 4

            self.runner = WPTRunner(self.port, self.host, "wptrunner_unittest", options,
                WPTRunnerTest.MockTestDownloader, WPTRunnerTest.MockWebDriver.create, spawn_wpt_func)

        def prepare_mock_files_for_run(self):
            self.host.filesystem.maybe_make_directory(self.port.wpt_metadata_directory())
            self.host.filesystem.write_text_file(
                "/mock-checkout/WebPlatformTests/MockPort/TestExpectations.json", "{}")
            self.host.filesystem.write_text_file(
                "/mock-checkout/WebPlatformTests/MockPort/TestManifest.ini", "{}")

    def test_prepare_wpt_checkout(self):
        # Tests the prepare_wpt_checkout() method with no WPT checkout specified in options.

        options, _ = parse_args([])
        instance = WPTRunnerTest.TestInstance(options)

        self.assertTrue(instance.runner.prepare_wpt_checkout())

        expected_wpt_checkout = "/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests"
        self.assertEquals(instance.runner._options.wpt_checkout, expected_wpt_checkout)
        self.assertTrue(instance.host.filesystem.isdir(expected_wpt_checkout))

    def test_prepare_wpt_checkout_specified_path(self):
        # Tests the prepare_wpt_checkout() method with WPT checkout specified in options.

        specified_wpt_checkout = "/mock-path/web-platform-tests"
        options, _ = parse_args(["--wpt-checkout", specified_wpt_checkout])
        instance = WPTRunnerTest.TestInstance(options)
        instance.host.filesystem.maybe_make_directory(specified_wpt_checkout)

        self.assertTrue(instance.runner.prepare_wpt_checkout())
        self.assertEquals(instance.runner._options.wpt_checkout, specified_wpt_checkout)

    def test_generate_metadata_directory(self):
        # Tests the _generate_metadata_directory() method. TestExpectations.json file is mocked with
        # TEST_EXPECTATIONS_JSON_CONTENT value as contents. Expected metadata is generated under
        # /mock-metadata and should correspond to files and files' content in the
        # EXPECTED_TEST_MANIFESTS dictionary.

        expectations_json_path = "/mock-checkout/WebPlatformTests/MockPort/TestExpectations.json"
        metadata_path = "/mock-metadata"

        options, _ = parse_args([])
        instance = WPTRunnerTest.TestInstance(options)

        instance.host.filesystem.write_text_file(
            "/mock-checkout/WebPlatformTests/MockPort/TestExpectations.json",
            TEST_EXPECTATIONS_JSON_CONTENT)

        # Specify a stale metadata file that should be removed before generation.
        stale_metadata_file = "/mock-metadata/test/stale_metadata_file.html.ini"
        instance.host.filesystem.write_text_file(
            stale_metadata_file, "[This file should be removed during generation]")

        self.assertTrue(instance.runner._generate_metadata_directory(metadata_path))

        # Check that the stale metadata file was indeed removed.
        self.assertFalse(instance.host.filesystem.exists(stale_metadata_file))

        for path, content in EXPECTED_TEST_MANIFESTS.items():
            manifest_path = instance.host.filesystem.join(metadata_path, path)
            self.assertTrue(instance.host.filesystem.isfile(manifest_path))
            self.assertEquals(instance.host.filesystem.read_text_file(manifest_path), content)

    def test_run(self):
        # Tests the run() method. Files are mocked to the point that helper methods don't fail.
        # Goal of this test is for the WPT spawn command to match the desired WPT directory and
        # arguments. No options or arguments are used.

        spawn_wpt_func = WPTRunnerTest.MockSpawnWPT(self,
            "/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests",
            ["run", "--webkit-port=MockPort", "--processes=4",
                "--metadata=/mock-path/mock-wpt-tests-metadata",
                "--manifest=/mock-path/mock-wpt-manifest.json",
                "--include-manifest=/mock-checkout/WebPlatformTests/MockPort/TestManifest.ini",
                "--webdriver-binary=/mock-webdriver/bin/webdriver",
                "--binary=/mock-webdriver/bin/browser",
                "--binary-arg=webdriver_arg1", "--binary-arg=webdriver_arg2", "webkit"])

        options, _ = parse_args([])
        instance = WPTRunnerTest.TestInstance(options, spawn_wpt_func)
        instance.prepare_mock_files_for_run()

        self.assertTrue(instance.runner.run([]))

    def test_run_with_specified_options(self):
        # Tests the run() method. Files are mocked to the point that helper methods don't fail.
        # Goal of this test is for the WPT spawn command to match the desired WPT directory and
        # arguments. Custom WPT checkout and child process count are specified. Note that the
        # WPT checkout doesn't have an impact on the resulting WPT argument list, as intended.
        specified_wpt_checkout = "/mock-path/web-platform-tests"
        specified_wpt_metadata = "/mock-path/wpt-metadata"
        specified_child_processes = 16

        spawn_wpt_func = WPTRunnerTest.MockSpawnWPT(self, specified_wpt_checkout,
            ["run", "--webkit-port=MockPort", "--processes=16",
                "--metadata=/mock-path/wpt-metadata",
                "--manifest=/mock-path/wpt-metadata/MANIFEST.json",
                "--include-manifest=/mock-path/wpt-metadata/MockPort/TestManifest.ini",
                "--webdriver-binary=/mock-webdriver/bin/webdriver",
                "--binary=/mock-webdriver/bin/browser",
                "--binary-arg=webdriver_arg1", "--binary-arg=webdriver_arg2", "webkit"])

        options, _ = parse_args(["--wpt-checkout", specified_wpt_checkout,
            "--wpt-metadata", specified_wpt_metadata,
            "--child-processes", specified_child_processes])
        instance = WPTRunnerTest.TestInstance(options, spawn_wpt_func)
        instance.prepare_mock_files_for_run()

        # Also create the mock WPT checkout and metadata directories.
        instance.host.filesystem.maybe_make_directory(specified_wpt_checkout)
        instance.host.filesystem.maybe_make_directory(specified_wpt_metadata)

        self.assertTrue(instance.runner.run([]))

    def test_run_with_args(self):
        # Tests the run() method. Files are mocked to the point that helper methods don't fail.
        # Goal of this test is for the WPT spawn command to match the desired WPT directory and
        # arguments. A custom two-element argument list is used. It's expected to be appended
        # to the resulting WPT argument list.

        specified_args = ["test1.html", "test2.html"]

        spawn_wpt_func = WPTRunnerTest.MockSpawnWPT(self,
            "/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests",
            ["run", "--webkit-port=MockPort", "--processes=4",
                "--metadata=/mock-path/mock-wpt-tests-metadata",
                "--manifest=/mock-path/mock-wpt-manifest.json",
                "--include-manifest=/mock-checkout/WebPlatformTests/MockPort/TestManifest.ini",
                "--webdriver-binary=/mock-webdriver/bin/webdriver",
                "--binary=/mock-webdriver/bin/browser",
                "--binary-arg=webdriver_arg1", "--binary-arg=webdriver_arg2", "webkit"] + specified_args)

        options, _ = parse_args([])
        instance = WPTRunnerTest.TestInstance(options, spawn_wpt_func)
        instance.prepare_mock_files_for_run()

        self.assertTrue(instance.runner.run(specified_args))

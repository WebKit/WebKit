#!/usr/bin/env python
# Copyright (C) 2010 Gabor Rapcsanyi <rgabor@inf.u-szeged.hu>, University of Szeged
# Copyright (C) 2010 Google Inc. All rights reserved.
#
# All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging
import unittest

from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.layout_tests.port import port_testcase
from webkitpy.layout_tests.port.webkit import WebKitPort, WebKitDriver
from webkitpy.layout_tests.port.config_mock import MockConfig
from webkitpy.tool.mocktool import MockOptions


class TestWebKitPort(WebKitPort):
    port_name = "testwebkitport"

    def __init__(self, symbols_string=None,
                 expectations_file=None, skips_file=None, host=None, config=None,
                 **kwargs):
        self.symbols_string = symbols_string  # Passing "" disables all staticly-detectable features.
        host = host or MockSystemHost()
        config = config or MockConfig()
        WebKitPort.__init__(self, host=host, config=config, **kwargs)

    def all_test_configurations(self):
        return [self.test_configuration()]

    def _webcore_symbols_string(self):
        return self.symbols_string

    def _tests_for_other_platforms(self):
        return ["media", ]

    def _tests_for_disabled_features(self):
        return ["accessibility", ]


class WebKitPortUnitTests(unittest.TestCase):
    def test_default_options(self):
        # The WebKit ports override new-run-webkit-test default options.
        options = MockOptions(pixel_tests=None, time_out_ms=None)
        port = WebKitPort(MockSystemHost(), options=options)
        self.assertEquals(port._options.pixel_tests, False)
        self.assertEquals(port._options.time_out_ms, 35000)

        # Note that we don't override options if specified by the user.
        options = MockOptions(pixel_tests=True, time_out_ms=6000)
        port = WebKitPort(MockSystemHost(), options=options)
        self.assertEquals(port._options.pixel_tests, True)
        self.assertEquals(port._options.time_out_ms, 6000)


class WebKitPortTest(port_testcase.PortTestCase):
    port_name = 'webkit'
    port_maker = TestWebKitPort

    def test_check_build(self):
        pass

    def test_driver_cmd_line(self):
        pass

    def test_baseline_search_path(self):
        pass

    def test_path_to_test_expectations_file(self):
        port = TestWebKitPort()
        port._options = MockOptions(webkit_test_runner=False)
        self.assertEqual(port.path_to_test_expectations_file(), '/mock-checkout/LayoutTests/platform/testwebkitport/test_expectations.txt')
        port._options = MockOptions(webkit_test_runner=True)
        self.assertEqual(port.path_to_test_expectations_file(), '/mock-checkout/LayoutTests/platform/testwebkitport/test_expectations.txt')

    def test_skipped_directories_for_symbols(self):
        # This first test confirms that the commonly found symbols result in the expected skipped directories.
        symbols_string = " ".join(["GraphicsLayer", "WebCoreHas3DRendering", "isXHTMLMPDocument", "fooSymbol"])
        expected_directories = set([
            "mathml",  # Requires MathMLElement
            "fast/canvas/webgl",  # Requires WebGLShader
            "compositing/webgl",  # Requires WebGLShader
            "http/tests/canvas/webgl",  # Requires WebGLShader
            "mhtml",  # Requires MHTMLArchive
            "fast/css/variables",  # Requires CSS Variables
        ])

        result_directories = set(TestWebKitPort(symbols_string, None)._skipped_tests_for_unsupported_features(test_list=['mathml/foo.html']))
        self.assertEqual(result_directories, expected_directories)

        # Test that the nm string parsing actually works:
        symbols_string = """
000000000124f498 s __ZZN7WebCore13GraphicsLayer12replaceChildEPS0_S1_E19__PRETTY_FUNCTION__
000000000124f500 s __ZZN7WebCore13GraphicsLayer13addChildAboveEPS0_S1_E19__PRETTY_FUNCTION__
000000000124f670 s __ZZN7WebCore13GraphicsLayer13addChildBelowEPS0_S1_E19__PRETTY_FUNCTION__
"""
        # Note 'compositing' is not in the list of skipped directories (hence the parsing of GraphicsLayer worked):
        expected_directories = set(['mathml', 'transforms/3d', 'compositing/webgl', 'fast/canvas/webgl', 'animations/3d', 'mhtml', 'http/tests/canvas/webgl', 'fast/css/variables'])
        result_directories = set(TestWebKitPort(symbols_string, None)._skipped_tests_for_unsupported_features(test_list=['mathml/foo.html']))
        self.assertEqual(result_directories, expected_directories)

    def test_skipped_directories_for_features(self):
        supported_features = ["Accelerated Compositing", "Foo Feature"]
        expected_directories = set(["animations/3d", "transforms/3d"])
        port = TestWebKitPort(None, supported_features)
        port._runtime_feature_list = lambda: supported_features
        result_directories = set(port._skipped_tests_for_unsupported_features(test_list=["animations/3d/foo.html"]))
        self.assertEqual(result_directories, expected_directories)

    def test_skipped_directories_for_features_no_matching_tests_in_test_list(self):
        supported_features = ["Accelerated Compositing", "Foo Feature"]
        expected_directories = set([])
        result_directories = set(TestWebKitPort(None, supported_features)._skipped_tests_for_unsupported_features(test_list=['foo.html']))
        self.assertEqual(result_directories, expected_directories)

    def test_skipped_tests_for_unsupported_features_empty_test_list(self):
        supported_features = ["Accelerated Compositing", "Foo Feature"]
        expected_directories = set([])
        result_directories = set(TestWebKitPort(None, supported_features)._skipped_tests_for_unsupported_features(test_list=None))
        self.assertEqual(result_directories, expected_directories)

    def test_skipped_layout_tests(self):
        self.assertEqual(TestWebKitPort(None, None).skipped_layout_tests(test_list=[]), set(['media']))

    def test_skipped_file_search_paths(self):
        port = TestWebKitPort()
        self.assertEqual(port._skipped_file_search_paths(), set(['testwebkitport']))
        port._name = "testwebkitport-version"
        self.assertEqual(port._skipped_file_search_paths(), set(['testwebkitport', 'testwebkitport-version']))
        port._options = MockOptions(webkit_test_runner=True)
        self.assertEqual(port._skipped_file_search_paths(), set(['testwebkitport', 'testwebkitport-version', 'testwebkitport-wk2', 'wk2']))
        port._options = MockOptions(additional_platform_directory=["internal-testwebkitport"])
        self.assertEqual(port._skipped_file_search_paths(), set(['testwebkitport', 'testwebkitport-version', 'internal-testwebkitport']))

    def test_root_option(self):
        port = TestWebKitPort()
        port._options = MockOptions(root='/foo')
        self.assertEqual(port._path_to_driver(), "/foo/DumpRenderTree")

    def test_test_expectations(self):
        # Check that we read the expectations file
        host = MockSystemHost()
        host.filesystem.write_text_file('/mock-checkout/LayoutTests/platform/testwebkitport/test_expectations.txt',
            'BUG_TESTEXPECTATIONS SKIP : fast/html/article-element.html = FAIL\n')
        port = TestWebKitPort(host=host)
        self.assertEqual(port.test_expectations(), 'BUG_TESTEXPECTATIONS SKIP : fast/html/article-element.html = FAIL\n')

    def test_build_driver(self):
        output = OutputCapture()
        port = TestWebKitPort()
        # Delay setting _executive to avoid logging during construction
        port._executive = MockExecutive(should_log=True)
        port._options = MockOptions(configuration="Release")  # This should not be necessary, but I think TestWebKitPort is actually reading from disk (and thus detects the current configuration).
        expected_stderr = "MOCK run_command: ['Tools/Scripts/build-dumprendertree', '--release'], cwd=/mock-checkout, env={'LC_ALL': 'C', 'MOCK_ENVIRON_COPY': '1'}\n"
        self.assertTrue(output.assert_outputs(self, port._build_driver, expected_stderr=expected_stderr, expected_logs=''))

        # Make sure when passed --webkit-test-runner we build the right tool.
        port._options = MockOptions(webkit_test_runner=True, configuration="Release")
        expected_stderr = "MOCK run_command: ['Tools/Scripts/build-dumprendertree', '--release'], cwd=/mock-checkout, env={'LC_ALL': 'C', 'MOCK_ENVIRON_COPY': '1'}\nMOCK run_command: ['Tools/Scripts/build-webkittestrunner', '--release'], cwd=/mock-checkout, env={'LC_ALL': 'C', 'MOCK_ENVIRON_COPY': '1'}\n"
        self.assertTrue(output.assert_outputs(self, port._build_driver, expected_stderr=expected_stderr, expected_logs=''))

        # Make sure we show the build log when --verbose is passed, which we simulate by setting the logging level to DEBUG.
        output.set_log_level(logging.DEBUG)
        port._options = MockOptions(configuration="Release")
        expected_stderr = "MOCK run_command: ['Tools/Scripts/build-dumprendertree', '--release'], cwd=/mock-checkout, env={'LC_ALL': 'C', 'MOCK_ENVIRON_COPY': '1'}\n"
        expected_logs = "Output of ['Tools/Scripts/build-dumprendertree', '--release']:\nMOCK output of child process\n"
        self.assertTrue(output.assert_outputs(self, port._build_driver, expected_stderr=expected_stderr, expected_logs=expected_logs))
        output.set_log_level(logging.INFO)

        # Make sure that failure to build returns False.
        port._executive = MockExecutive(should_log=True, should_throw=True)
        # Because WK2 currently has to build both webkittestrunner and DRT, if DRT fails, that's the only one it tries.
        expected_stderr = "MOCK run_command: ['Tools/Scripts/build-dumprendertree', '--release'], cwd=/mock-checkout, env={'LC_ALL': 'C', 'MOCK_ENVIRON_COPY': '1'}\n"
        expected_logs = "MOCK ScriptError\n\nMOCK output of child process\n"
        self.assertFalse(output.assert_outputs(self, port._build_driver, expected_stderr=expected_stderr, expected_logs=expected_logs))

    def _assert_config_file_for_platform(self, port, platform, config_file):
        self.assertEquals(port._apache_config_file_name_for_platform(platform), config_file)

    def test_linux_distro_detection(self):
        port = TestWebKitPort()
        self.assertFalse(port._is_redhat_based())
        self.assertFalse(port._is_debian_based())

        port._filesystem = MockFileSystem({'/etc/redhat-release': ''})
        self.assertTrue(port._is_redhat_based())
        self.assertFalse(port._is_debian_based())

        port._filesystem = MockFileSystem({'/etc/debian_version': ''})
        self.assertFalse(port._is_redhat_based())
        self.assertTrue(port._is_debian_based())

    def test_apache_config_file_name_for_platform(self):
        port = TestWebKitPort()
        self._assert_config_file_for_platform(port, 'cygwin', 'cygwin-httpd.conf')

        self._assert_config_file_for_platform(port, 'linux2', 'apache2-httpd.conf')
        self._assert_config_file_for_platform(port, 'linux3', 'apache2-httpd.conf')

        port._is_redhat_based = lambda: True
        self._assert_config_file_for_platform(port, 'linux2', 'fedora-httpd.conf')

        port = TestWebKitPort()
        port._is_debian_based = lambda: True
        self._assert_config_file_for_platform(port, 'linux2', 'apache2-debian-httpd.conf')

        self._assert_config_file_for_platform(port, 'mac', 'apache2-httpd.conf')
        self._assert_config_file_for_platform(port, 'win32', 'apache2-httpd.conf')  # win32 isn't a supported sys.platform.  AppleWin/WinCairo/WinCE ports all use cygwin.
        self._assert_config_file_for_platform(port, 'barf', 'apache2-httpd.conf')

    def test_path_to_apache_config_file(self):
        port = TestWebKitPort()
        # Mock out _apache_config_file_name_for_platform to ignore the passed sys.platform value.
        port._apache_config_file_name_for_platform = lambda platform: 'httpd.conf'
        self.assertEquals(port._path_to_apache_config_file(), '/mock-checkout/LayoutTests/http/conf/httpd.conf')


class MockServerProcess(object):
    def __init__(self, lines=None):
        self.timed_out = False
        self.lines = lines or []
        self.crashed = False

    def has_crashed(self):
        return self.crashed

    def read_stdout_line(self, deadline):
        return self.lines.pop(0) + "\n"

    def read_stdout(self, deadline, size):
        # read_stdout doesn't actually function on lines, but this is sufficient for our testing.
        line = self.lines.pop(0)
        assert len(line) == size
        return line

    def read_either_stdout_or_stderr_line(self, deadline):
        # FIXME: We should have tests which intermix stderr and stdout lines.
        return self.read_stdout_line(deadline), None


class WebKitDriverTest(unittest.TestCase):
    def test_read_block(self):
        port = TestWebKitPort()
        driver = WebKitDriver(port, 0, pixel_tests=False)
        driver._server_process = MockServerProcess([
            'ActualHash: foobar',
            'Content-Type: my_type',
            'Content-Transfer-Encoding: none',
            "#EOF",
        ])
        content_block = driver._read_block(0)
        self.assertEquals(content_block.content_type, 'my_type')
        self.assertEquals(content_block.encoding, 'none')
        self.assertEquals(content_block.content_hash, 'foobar')

    def test_read_binary_block(self):
        port = TestWebKitPort()
        driver = WebKitDriver(port, 0, pixel_tests=True)
        driver._server_process = MockServerProcess([
            'ActualHash: actual',
            'ExpectedHash: expected',
            'Content-Type: image/png',
            'Content-Length: 8',
            "12345678",
            "#EOF",
        ])
        content_block = driver._read_block(0)
        self.assertEquals(content_block.content_type, 'image/png')
        self.assertEquals(content_block.content_hash, 'actual')
        self.assertEquals(content_block.content, '12345678')
        self.assertEquals(content_block.decoded_content, '12345678')

    def test_no_timeout(self):
        port = TestWebKitPort()
        driver = WebKitDriver(port, 0, pixel_tests=True, no_timeout=True)
        self.assertEquals(driver.cmd_line(True, []), ['/mock-build/DumpRenderTree', '--no-timeout', '--pixel-tests', '-'])

    def test_check_for_driver_crash(self):
        port = TestWebKitPort()
        driver = WebKitDriver(port, 0, pixel_tests=True)

        class FakeServerProcess(object):
            def __init__(self, crashed):
                self.crashed = crashed

            def pid(self):
                return 1234

            def name(self):
                return 'FakeServerProcess'

            def has_crashed(self):
                return self.crashed

        def assert_crash(driver, error_line, crashed, name, pid):
            self.assertEquals(driver._check_for_driver_crash(error_line), crashed)
            self.assertEquals(driver._crashed_process_name, name)
            self.assertEquals(driver._crashed_pid, pid)

        driver._server_process = FakeServerProcess(False)
        assert_crash(driver, '', False, None, None)

        driver._crashed_process_name = None
        driver._crashed_pid = None
        driver._server_process = FakeServerProcess(False)
        assert_crash(driver, '#CRASHED\n', True, 'FakeServerProcess', 1234)

        driver._crashed_process_name = None
        driver._crashed_pid = None
        driver._server_process = FakeServerProcess(False)
        assert_crash(driver, '#CRASHED - WebProcess\n', True, 'WebProcess', None)

        driver._crashed_process_name = None
        driver._crashed_pid = None
        driver._server_process = FakeServerProcess(False)
        assert_crash(driver, '#CRASHED - WebProcess (pid 8675)\n', True, 'WebProcess', 8675)

        driver._crashed_process_name = None
        driver._crashed_pid = None
        driver._server_process = FakeServerProcess(True)
        assert_crash(driver, '', True, 'FakeServerProcess', 1234)

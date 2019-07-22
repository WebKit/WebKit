# Copyright (C) 2010 Google Inc. All rights reserved.
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

import unittest

from webkitpy.common.system.systemhost_mock import MockSystemHost

from webkitpy.port import Port, Driver, DriverOutput
from webkitpy.port.server_process_mock import MockServerProcess
from webkitpy.thirdparty.mock import patch

# FIXME: remove the dependency on TestWebKitPort
from webkitpy.port.port_testcase import TestWebKitPort

from webkitpy.tool.mocktool import MockOptions

import os
import sys


class DriverOutputTest(unittest.TestCase):
    def test_strip_metrics(self):
        patterns = [
            ('RenderView at (0,0) size 800x600', 'RenderView '),
            ('text run at (0,0) width 100: "some text"', '"some text"'),
            ('RenderBlock {HTML} at (0,0) size 800x600', 'RenderBlock {HTML} '),
            ('RenderBlock {INPUT} at (29,3) size 12x12 [color=#000000]', 'RenderBlock {INPUT}'),

            ('RenderBlock (floating) {DT} at (5,5) size 79x310 [border: (5px solid #000000)]',
            'RenderBlock (floating) {DT} [border: px solid #000000)]'),

            ('\n    "truncate text    "\n', '\n    "truncate text"\n'),

            ('RenderText {#text} at (0,3) size 41x12\n    text run at (0,3) width 41: "whimper "\n',
            'RenderText {#text} \n    "whimper"\n'),

            ("""text run at (0,0) width 109: ".one {color: green;}"
          text run at (109,0) width 0: " "
          text run at (0,17) width 81: ".1 {color: red;}"
          text run at (81,17) width 0: " "
          text run at (0,34) width 102: ".a1 {color: green;}"
          text run at (102,34) width 0: " "
          text run at (0,51) width 120: "P.two {color: purple;}"
          text run at (120,51) width 0: " "\n""",
            '".one {color: green;}  .1 {color: red;}  .a1 {color: green;}  P.two {color: purple;}"\n'),

            ('text-- other text', 'text--other text'),

            (' some output   "truncate trailing spaces at end of line after text"   \n',
            ' some output   "truncate trailing spaces at end of line after text"\n'),

            (r'scrollWidth 120', r'scrollWidth'),
            (r'scrollHeight 120', r'scrollHeight'),
        ]

        for pattern in patterns:
            driver_output = DriverOutput(pattern[0], None, None, None)
            driver_output.strip_metrics()
            self.assertEqual(driver_output.text, pattern[1])


class DriverTest(unittest.TestCase):
    def make_port(self, host=None, options=None):
        port = Port(host or MockSystemHost(), 'test', options or MockOptions(configuration='Release'))
        port._config.build_directory = lambda configuration: '/mock-build'
        return port

    def _assert_wrapper(self, wrapper_string, expected_wrapper):
        wrapper = Driver(self.make_port(options=MockOptions(wrapper=wrapper_string)), None, pixel_tests=False)._command_wrapper()
        self.assertEqual(wrapper, expected_wrapper)

    def test_command_wrapper(self):
        self._assert_wrapper(None, [])
        self._assert_wrapper("valgrind", ["valgrind"])

        # Validate that shlex works as expected.
        command_with_spaces = "valgrind --smc-check=\"check with spaces!\" --foo"
        expected_parse = ["valgrind", "--smc-check=check with spaces!", "--foo"]
        self._assert_wrapper(command_with_spaces, expected_parse)

    def test_profiler_as_wrapper(self):
        driver = Driver(self.make_port(MockSystemHost(os_name='linux'), MockOptions(profile=True, profiler='perf')), None, False)
        self.assertEqual(driver._command_wrapper(), ['perf', 'record', '-g', '--output', '/mock-build/layout-test-results/test.data'])

    def test_profiler_and_wrapper(self):
        driver = Driver(self.make_port(MockSystemHost(os_name='linux'), MockOptions(profile=True, profiler='perf', wrapper='valgrind')), None, False)
        self.assertEqual(driver._command_wrapper(), ['valgrind', 'perf', 'record', '-g', '--output', '/mock-build/layout-test-results/test.data'])

    def test_test_to_uri(self):
        port = self.make_port()
        driver = Driver(port, None, pixel_tests=False)
        self.assertEqual(driver.test_to_uri('foo/bar.html'), 'file://%s/foo/bar.html' % port.layout_tests_dir())
        self.assertEqual(driver.test_to_uri('http/tests/foo.html'), 'http://127.0.0.1:8000/foo.html')
        self.assertEqual(driver.test_to_uri('http/tests/ssl/bar.html'), 'https://127.0.0.1:8443/ssl/bar.html')
        self.assertEqual(driver.test_to_uri('imported/w3c/web-platform-tests/foo/bar.html'), 'http://localhost:8800/foo/bar.html')
        self.assertEqual(driver.test_to_uri('imported/w3c/web-platform-tests/foo/bar.https.html'), 'https://localhost:9443/foo/bar.https.html')
        self.assertEqual(driver.test_to_uri('http/wpt/bar2.html'), 'http://localhost:8800/WebKit/bar2.html')
        self.assertEqual(driver.test_to_uri('http/wpt/bar2.https.html'), 'https://localhost:9443/WebKit/bar2.https.html')

    def test_uri_to_test(self):
        port = self.make_port()
        driver = Driver(port, None, pixel_tests=False)
        self.assertEqual(driver.uri_to_test('file://%s/foo/bar.html' % port.layout_tests_dir()), 'foo/bar.html')
        self.assertEqual(driver.uri_to_test('http://127.0.0.1:8000/foo.html'), 'http/tests/foo.html')
        self.assertEqual(driver.uri_to_test('https://127.0.0.1:8443/ssl/bar.html'), 'http/tests/ssl/bar.html')
        self.assertEqual(driver.uri_to_test('https://127.0.0.1:8443/ssl/bar.https.html'), 'http/tests/ssl/bar.https.html')
        self.assertEqual(driver.uri_to_test('http://localhost:8800/foo/bar.html'), 'imported/w3c/web-platform-tests/foo/bar.html')
        self.assertEqual(driver.uri_to_test('http://localhost:8800/WebKit/bar2.html'), 'http/wpt/bar2.html')
        self.assertEqual(driver.uri_to_test('https://localhost:9443/WebKit/bar2.https.html'), 'http/wpt/bar2.https.html')

    def test_read_block(self):
        port = TestWebKitPort()
        driver = Driver(port, 0, pixel_tests=False)
        driver._server_process = MockServerProcess(lines=[
            'ActualHash: foobar',
            'Content-Type: my_type',
            'Content-Transfer-Encoding: none',
            "#EOF",
        ])
        content_block = driver._read_block(0, "")
        self.assertEqual(content_block.content_type, 'my_type')
        self.assertEqual(content_block.encoding, 'none')
        self.assertEqual(content_block.content_hash, 'foobar')
        # We should only poll once for each line.
        self.assertEqual(driver._server_process.number_of_times_polled, 4)
        driver._server_process = None

    def test_read_block_crashed_process(self):
        port = TestWebKitPort()
        driver = Driver(port, 0, pixel_tests=False)
        driver._server_process = MockServerProcess(
            crashed=True,
            lines=[
                'ActualHash: foobar',
                'Content-Type: my_type',
                'Content-Transfer-Encoding: none',
                '#EOF',
            ])

        content_block = driver._read_block(0, "")
        self.assertEqual(content_block.content_type, None)
        self.assertEqual(content_block.encoding, None)
        self.assertEqual(content_block.content_hash, None)
        driver._server_process = None

    def test_read_binary_block(self):
        port = TestWebKitPort()
        driver = Driver(port, 0, pixel_tests=True)
        driver._server_process = MockServerProcess(lines=[
            'ActualHash: actual',
            'ExpectedHash: expected',
            'Content-Type: image/png',
            'Content-Length: 9',
            "12345678",
            "#EOF",
        ])
        content_block = driver._read_block(0, "")
        self.assertEqual(content_block.content_type, 'image/png')
        self.assertEqual(content_block.content_hash, 'actual')
        self.assertEqual(content_block.content, '12345678\n')
        self.assertEqual(content_block.decoded_content, '12345678\n')
        driver._server_process = None

    def test_read_base64_block(self):
        port = TestWebKitPort()
        driver = Driver(port, 0, pixel_tests=True)
        driver._server_process = MockServerProcess(lines=[
            'ActualHash: actual',
            'ExpectedHash: expected',
            'Content-Type: image/png',
            'Content-Transfer-Encoding: base64',
            'Content-Length: 12',
            'MTIzNDU2NzgK#EOF',
        ])
        content_block = driver._read_block(0, "")
        self.assertEqual(content_block.content_type, 'image/png')
        self.assertEqual(content_block.content_hash, 'actual')
        self.assertEqual(content_block.encoding, 'base64')
        self.assertEqual(content_block.content, 'MTIzNDU2NzgK')
        self.assertEqual(content_block.decoded_content, '12345678\n')

    def test_no_timeout(self):
        port = TestWebKitPort()
        port._config.build_directory = lambda configuration: '/mock-build'
        driver = Driver(port, 0, pixel_tests=True, no_timeout=True)
        if sys.platform.startswith('win'):
            self.assertEqual(driver.cmd_line(True, []), ['/mock-build/DumpRenderTree.exe', '--no-timeout', '-'])
        else:
            self.assertEqual(driver.cmd_line(True, []), ['/mock-build/DumpRenderTree', '--no-timeout', '-'])

    def test_check_for_driver_crash(self):
        port = TestWebKitPort()
        driver = Driver(port, 0, pixel_tests=True)

        class FakeServerProcess(object):
            def __init__(self, crashed):
                self.crashed = crashed

            def pid(self):
                return 1234

            def system_pid(self):
                return self.pid()

            def process_name(self):
                return 'FakeServerProcess'

            def has_crashed(self):
                return self.crashed

            def stop(self, timeout):
                pass

            def write(self, bytes, ignore_crash=False):
                pass

        def assert_crash(driver, error_line, crashed, name, pid, unresponsive=False):
            self.assertEqual(driver._check_for_driver_crash_or_unresponsiveness(error_line), crashed)
            self.assertEqual(driver._crashed_process_name, name)
            self.assertEqual(driver._crashed_pid, pid)
            self.assertEqual(driver._driver_timed_out, unresponsive)
            driver.stop()

        driver._server_process = FakeServerProcess(False)
        assert_crash(driver, '', False, None, None)

        driver._crashed_process_name = None
        driver._crashed_pid = None
        driver._server_process = FakeServerProcess(False)
        driver._driver_timed_out = False
        assert_crash(driver, '#CRASHED\n', True, 'FakeServerProcess', 1234)

        driver._crashed_process_name = None
        driver._crashed_pid = None
        driver._server_process = FakeServerProcess(False)
        driver._driver_timed_out = False
        assert_crash(driver, '#CRASHED - WebProcess\n', True, 'WebProcess', None)

        driver._crashed_process_name = None
        driver._crashed_pid = None
        driver._server_process = FakeServerProcess(False)
        driver._driver_timed_out = False
        assert_crash(driver, '#CRASHED - WebProcess (pid 8675)\n', True, 'WebProcess', 8675)

        driver._crashed_process_name = None
        driver._crashed_pid = None
        driver._server_process = FakeServerProcess(False)
        driver._driver_timed_out = False
        assert_crash(driver, '#PROCESS UNRESPONSIVE - WebProcess (pid 8675)\n', True, None, None, True)

        driver._crashed_process_name = None
        driver._crashed_pid = None
        driver._server_process = FakeServerProcess(False)
        driver._driver_timed_out = False
        assert_crash(driver, '#CRASHED - renderer (pid 8675)\n', True, 'renderer', 8675)

        driver._crashed_process_name = None
        driver._crashed_pid = None
        driver._server_process = FakeServerProcess(True)
        driver._driver_timed_out = False
        assert_crash(driver, '', True, 'FakeServerProcess', 1234)

    def test_creating_a_port_does_not_write_to_the_filesystem(self):
        port = TestWebKitPort()
        driver = Driver(port, 0, pixel_tests=True)
        self.assertEqual(port._filesystem.written_files, {})
        self.assertEqual(port._filesystem.last_tmpdir, None)

    def test_stop_cleans_up_properly(self):
        port = TestWebKitPort()
        port._test_runner_process_constructor = MockServerProcess
        driver = Driver(port, 0, pixel_tests=True)
        driver.start(True, [])
        last_tmpdir = port._filesystem.last_tmpdir
        self.assertNotEquals(last_tmpdir, None)
        driver.stop()
        self.assertFalse(port._filesystem.isdir(last_tmpdir))

    def test_two_starts_cleans_up_properly(self):
        port = TestWebKitPort()
        port._test_runner_process_constructor = MockServerProcess
        driver = Driver(port, 0, pixel_tests=True)
        driver.start(True, [])
        last_tmpdir = port._filesystem.last_tmpdir
        driver._start(True, [])
        self.assertFalse(port._filesystem.isdir(last_tmpdir))

    def test_start_actually_starts(self):
        port = TestWebKitPort()
        port._test_runner_process_constructor = MockServerProcess
        driver = Driver(port, 0, pixel_tests=True)
        driver.start(True, [])
        self.assertTrue(driver._server_process.started)

    def test__append_environment_variable_path(self):
        port = self.make_port()
        driver = Driver(port, None, pixel_tests=False)

        environment = {}
        variable = 'DYLD_FAKE_PATH'
        path = '/tmp/fake-dyld-path'
        driver._append_environment_variable_path(environment, variable, path)
        self.assertEqual(environment[variable], path)

        base_value = ''
        environment[variable] = base_value
        driver._append_environment_variable_path(environment, variable, path)
        self.assertEqual(environment[variable], base_value + os.pathsep + path)

        base_value = '/usr/lib'
        environment[variable] = base_value
        driver._append_environment_variable_path(environment, variable, path)
        self.assertEqual(environment[variable], base_value + os.pathsep + path)

        base_value = os.pathsep.join(['/usr/lib', '/usr/local/lib'])
        environment[variable] = base_value
        driver._append_environment_variable_path(environment, variable, path)
        self.assertEqual(environment[variable], base_value + os.pathsep + path)

    def test_setup_environ_for_test(self):
        environment_user = {}
        environment_user['WEBKIT_OUTPUTDIR'] = '/opt/webkit/WebKitBuild/Release'
        environment_user['FOO'] = 'BAR'
        with patch('os.environ', environment_user):
            port = self.make_port()
            driver = Driver(port, None, pixel_tests=False)
            environment_driver_test = driver._setup_environ_for_test()
            self.assertNotIn('FOO', environment_driver_test)
            self.assertIn('WEBKIT_OUTPUTDIR', environment_driver_test)
            self.assertEqual(environment_user['WEBKIT_OUTPUTDIR'], environment_driver_test['WEBKIT_OUTPUTDIR'])

    def test_setup_environ_base_vars(self):
        # This are essential environment variables that should be copied
        # as part of base:setup_environ_for_server for all drivers
        environ_keep_yes = {'PATH': '/bin:/usr/sbin:/usr/bin',
                           'WEBKIT_TESTFONTS': '/opt/webkit/WebKitBuild/WKTestFonts',
                           'WEBKIT_OUTPUTDIR': '/opt/webkit/WebKitBuild/Release',
                           'LANG': 'en_US.utf8'}
        # This are environment variables that should be copied
        # on the driver (wayland, x11). But not in the base driver.
        environ_keep_no = {'DISPLAY': ':0.0',
                          'XAUTHORITY': '/home/igalia/.Xauthority',
                          'WAYLAND_DISPLAY': 'wayland-0',
                          'WAYLAND_SOCKET': 'wayland-socket-0',
                          'GDK_BACKEND': 'x11'}
        environment_user = dict(environ_keep_yes.items() + environ_keep_no.items())
        with patch('os.environ', environment_user):
            port = self.make_port()
            driver = Driver(port, None, pixel_tests=False)
            environment_driver_test = driver._setup_environ_for_test()
            for var in environ_keep_no.keys():
                    self.assertNotIn(var, environment_driver_test)
            for var in environ_keep_yes.keys():
                    self.assertIn(var, environment_driver_test)
                    self.assertEqual(environment_driver_test[var], environ_keep_yes[var])

    def test_setup_environ_without_starting_driver(self):
        environ_user = {'WEBKIT_OUTPUTDIR': '/opt/webkit/WebKitBuild/Release'}
        with patch('os.environ', environ_user):
            port = self.make_port()
            driver = Driver(port, None, pixel_tests=False)
            self.assertEqual(driver._driver_tempdir, None)
            environ_driver = driver._setup_environ_for_test()
            self.assertNotEqual(driver._driver_tempdir, None)
            self.assertTrue(port._filesystem.isdir(str(driver._driver_tempdir)))
            for var in environ_driver:
                self.assertNotEqual(var, None)
                self.assertNotEqual(environ_driver[var], None)

    def test_create_temporal_home_dir(self):
        environ_user = {'HOME': '/home/igalia'}
        with patch('os.environ', environ_user), patch('sys.platform', 'linux2'):
            port = self.make_port()
            port._test_runner_process_constructor = MockServerProcess
            driver = Driver(port, None, pixel_tests=False)
            driver.start(True, [])
            environ_driver = driver._setup_environ_for_test()
            self.assertNotEquals(environ_driver['HOME'], environ_user['HOME'])
            self.assertIn(str(driver._driver_tempdir), environ_driver['HOME'])
            self.assertNotIn(str(driver._driver_tempdir), environ_user['HOME'])
            self.assertTrue(port._filesystem.isdir(environ_driver['HOME']))

# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

"""Unit testing base class for Port implementations."""

import errno
import logging
import os
import socket
import sys
import time
import unittest

from contextlib import contextmanager

from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.common.version_name_map import INTERNAL_TABLE
from webkitpy.port.base import Port
from webkitpy.port.config import apple_additions, clear_cached_configuration
from webkitpy.port.image_diff import ImageDiffer
from webkitpy.port.server_process_mock import MockServerProcess
from webkitpy.layout_tests.servers import http_server_base
from webkitpy.tool.mocktool import MockOptions


# FIXME: get rid of this fixture
class TestWebKitPort(Port):
    port_name = "testwebkitport"

    def __init__(self, port_name=None,
                 expectations_file=None, skips_file=None, host=None, config=None,
                 **kwargs):
        port_name = port_name or TestWebKitPort.port_name
        host = host or MockSystemHost()
        super(TestWebKitPort, self).__init__(host, port_name=port_name, **kwargs)

    def all_test_configurations(self):
        return [self.test_configuration()]

    def _tests_for_other_platforms(self, **kwargs):
        return ["media", ]

    def _tests_for_disabled_features(self):
        return ["accessibility", ]


@contextmanager
def bind_mock_apple_additions():
    from webkitpy.common.version_name_map import PUBLIC_TABLE, VersionNameMap

    class MockAppleAdditions(object):

        @staticmethod
        def layout_tests_path():
            return '/additional_testing_path/'

        @staticmethod
        def version_name_mapping(platform=None):
            result = VersionNameMap(platform)
            result.mapping[INTERNAL_TABLE] = {}
            for platform in result.mapping[PUBLIC_TABLE]:
                result.mapping[INTERNAL_TABLE][platform] = {}
                for name, version in result.mapping[PUBLIC_TABLE][platform].iteritems():
                    result.mapping[INTERNAL_TABLE][platform]['add-' + name] = version
            return result

    # apple_additions is a memoized function. Take advantage of this fact and manipulate the cache
    # to temporarily return a mocked result
    apple_additions._results_cache[()] = MockAppleAdditions
    VersionNameMap.map._results_cache = {}
    yield
    apple_additions._results_cache[()] = None
    VersionNameMap.map._results_cache = {}


class PortTestCase(unittest.TestCase):
    """Tests that all Port implementations must pass."""
    HTTP_PORTS = (8000, 8080, 8443)
    WEBSOCKET_PORTS = (8880,)

    # Subclasses override this to point to their Port subclass.
    os_name = None
    os_version = None
    port_maker = TestWebKitPort
    port_name = None
    disable_setup = False

    def make_port(self, host=None, port_name=None, options=None, os_name=None, os_version=None, **kwargs):
        host = host or MockSystemHost(os_name=(os_name or self.os_name), os_version=(os_version or self.os_version))
        options = options or MockOptions(configuration='Release')
        port_name = port_name or self.port_name
        port_name = self.port_maker.determine_full_port_name(host, options, port_name)
        port = self.port_maker(host, port_name, options=options, **kwargs)
        port._config.build_directory = lambda configuration: '/mock-build'
        return port

    def test_default_timeout_ms(self):
        self.assertEqual(self.make_port(options=MockOptions(configuration='Release')).default_timeout_ms(), 30000)
        self.assertEqual(self.make_port(options=MockOptions(configuration='Debug')).default_timeout_ms(), 30000)

    def test_default_pixel_tests(self):
        self.assertEqual(self.make_port().default_pixel_tests(), False)

    def test_driver_cmd_line(self):
        port = self.make_port()
        self.assertTrue(len(port.driver_cmd_line_for_logging()))

        options = MockOptions(additional_drt_flag=['--foo=bar', '--foo=baz'])
        port = self.make_port(options=options)
        cmd_line = port.driver_cmd_line_for_logging()
        self.assertTrue('--foo=bar' in cmd_line)
        self.assertTrue('--foo=baz' in cmd_line)

    def assert_servers_are_down(self, host, ports):
        for port in ports:
            try:
                test_socket = socket.socket()
                test_socket.connect((host, port))
                self.fail()
            except IOError as e:
                self.assertTrue(e.errno in (errno.ECONNREFUSED, errno.ECONNRESET))
            finally:
                test_socket.close()

    def assert_servers_are_up(self, host, ports):
        for port in ports:
            try:
                test_socket = socket.socket()
                test_socket.connect((host, port))
            except IOError as e:
                self.fail('failed to connect to %s:%d' % (host, port))
            finally:
                test_socket.close()

    def integration_test_check_sys_deps(self):
        port = self.make_port()
        # Only checking that no exception is raised.
        port.check_sys_deps()

    def integration_test_helper(self):
        port = self.make_port()
        # Only checking that no exception is raised.
        port.start_helper()
        port.stop_helper()

    def integration_test_http_server__normal(self):
        port = self.make_port()
        self.assert_servers_are_down('localhost', self.HTTP_PORTS)
        port.start_http_server()
        self.assert_servers_are_up('localhost', self.HTTP_PORTS)
        port.stop_http_server()
        self.assert_servers_are_down('localhost', self.HTTP_PORTS)

    def integration_test_http_server__fails(self):
        port = self.make_port()
        # Test that if a port isn't available, the call fails.
        for port_number in self.HTTP_PORTS:
            test_socket = socket.socket()
            try:
                try:
                    test_socket.bind(('localhost', port_number))
                except socket.error as e:
                    if e.errno in (errno.EADDRINUSE, errno.EALREADY):
                        self.fail('could not bind to port %d' % port_number)
                    raise
                try:
                    port.start_http_server()
                    self.fail('should not have been able to start the server while bound to %d' % port_number)
                except http_server_base.ServerError as e:
                    pass
            finally:
                port.stop_http_server()
                test_socket.close()

        # Test that calling start() twice fails.
        try:
            port.start_http_server()
            self.assertRaises(AssertionError, port.start_http_server)
        finally:
            port.stop_http_server()

    def integration_test_http_server__two_servers(self):
        # Test that calling start() on two different ports causes the
        # first port to be treated as stale and killed.
        port = self.make_port()
        # Test that if a port isn't available, the call fails.
        port.start_http_server()
        new_port = self.make_port()
        try:
            new_port.start_http_server()

            # Check that the first server was killed.
            self.assertFalse(port._executive.check_running_pid(port._http_server._pid))

            # Check that there is something running.
            self.assert_servers_are_up('localhost', self.HTTP_PORTS)

            # Test that calling stop() on a killed server is harmless.
            port.stop_http_server()
        finally:
            port.stop_http_server()
            new_port.stop_http_server()

            # Test that calling stop() twice is harmless.
            new_port.stop_http_server()

    def integration_test_image_diff(self):
        port = self.make_port()
        # FIXME: This test will never run since we are using a MockFilesystem for these tests!?!?
        if not port.check_image_diff():
            # The port hasn't been built - don't run the tests.
            return

        dir = port.layout_tests_dir()
        file1 = port._filesystem.join(dir, 'fast', 'css', 'button_center.png')
        contents1 = port._filesystem.read_binary_file(file1)
        file2 = port._filesystem.join(dir, 'fast', 'css',
                                      'remove-shorthand-expected.png')
        contents2 = port._filesystem.read_binary_file(file2)
        tmpfd, tmpfile = port._filesystem.open_binary_tempfile('')
        tmpfd.close()

        self.assertFalse(port.diff_image(contents1, contents1)[0])
        self.assertTrue(port.diff_image(contents1, contents2)[0])

        self.assertTrue(port.diff_image(contents1, contents2, tmpfile)[0])

        port._filesystem.remove(tmpfile)

    def test_diff_image__missing_both(self):
        port = self.make_port()
        self.assertFalse(port.diff_image(None, None)[0])
        self.assertFalse(port.diff_image(None, '')[0])
        self.assertFalse(port.diff_image('', None)[0])

        self.assertFalse(port.diff_image('', '')[0])

    def test_diff_image__missing_actual(self):
        port = self.make_port()
        self.assertTrue(port.diff_image(None, 'foo')[0])
        self.assertTrue(port.diff_image('', 'foo')[0])

    def test_diff_image__missing_expected(self):
        port = self.make_port()
        self.assertTrue(port.diff_image('foo', None)[0])
        self.assertTrue(port.diff_image('foo', '')[0])

    def test_diff_image(self):
        port = self.make_port()
        self.proc = None

        def make_proc(port, nm, cmd, env):
            self.proc = MockServerProcess(port, nm, cmd, env, lines=['diff: 100% failed\n', 'diff: 100% failed\n'])
            return self.proc

        # FIXME: Can't pretend to run setup for some ports, so just skip this test.
        if self.disable_setup:
            return

        port._server_process_constructor = make_proc
        port.setup_test_run()

        # First test the case of not using the JHBuild wrapper.
        self.assertFalse(port._should_use_jhbuild())

        self.assertEqual(port.diff_image('foo', 'bar'), ('', 100.0, None))
        self.assertEqual(self.proc.cmd, [port._path_to_image_diff(), "--tolerance", "0.1"])
        self.assertEqual(port.diff_image('foo', 'bar', None), ('', 100.0, None))
        self.assertEqual(self.proc.cmd, [port._path_to_image_diff(), "--tolerance", "0.1"])
        self.assertEqual(port.diff_image('foo', 'bar', 0), ('', 100.0, None))
        self.assertEqual(self.proc.cmd, [port._path_to_image_diff(), "--tolerance", "0"])

        # Now test the case of using JHBuild wrapper.
        port._filesystem.maybe_make_directory(port.path_from_webkit_base('WebKitBuild', 'Dependencies%s' % port.port_name.upper()))
        self.assertTrue(port._should_use_jhbuild())

        self.assertEqual(port.diff_image('foo', 'bar'), ('', 100.0, None))
        self.assertEqual(self.proc.cmd, port._jhbuild_wrapper + [port._path_to_image_diff(), "--tolerance", "0.1"])
        self.assertEqual(port.diff_image('foo', 'bar', None), ('', 100.0, None))
        self.assertEqual(self.proc.cmd, port._jhbuild_wrapper + [port._path_to_image_diff(), "--tolerance", "0.1"])
        self.assertEqual(port.diff_image('foo', 'bar', 0), ('', 100.0, None))
        self.assertEqual(self.proc.cmd, port._jhbuild_wrapper + [port._path_to_image_diff(), "--tolerance", "0"])

        port.clean_up_test_run()
        self.assertTrue(self.proc.stopped)
        self.assertEqual(port._image_differ, None)

    def test_diff_image_passed(self):
        port = self.make_port()
        port._server_process_constructor = lambda port, nm, cmd, env: MockServerProcess(lines=['diff: 0% passed\n'])
        image_differ = ImageDiffer(port)
        self.assertEqual(image_differ.diff_image('foo', 'bar', 0.1), (None, 0, None))

    def test_diff_image_failed(self):
        port = self.make_port()
        port._server_process_constructor = lambda port, nm, cmd, env: MockServerProcess(lines=['diff: 100% failed\n'])
        image_differ = ImageDiffer(port)
        self.assertEqual(image_differ.diff_image('foo', 'bar', 0.1), ('', 100.0, None))

    def test_diff_image_crashed(self):
        port = self.make_port()
        self.proc = None

        def make_proc(port, nm, cmd, env):
            self.proc = MockServerProcess(port, nm, cmd, env, crashed=True)
            return self.proc

        # FIXME: Can't pretend to run setup for some ports, so just skip this test.
        if self.disable_setup:
            return

        port._server_process_constructor = make_proc
        port.setup_test_run()
        self.assertEqual(port.diff_image('foo', 'bar'), ('', 0, 'ImageDiff crashed\n'))
        port.clean_up_test_run()

    def integration_test_websocket_server__normal(self):
        port = self.make_port()
        self.assert_servers_are_down('localhost', self.WEBSOCKET_PORTS)
        port.start_websocket_server()
        self.assert_servers_are_up('localhost', self.WEBSOCKET_PORTS)
        port.stop_websocket_server()
        self.assert_servers_are_down('localhost', self.WEBSOCKET_PORTS)

    def integration_test_websocket_server__fails(self):
        port = self.make_port()

        # Test that start() fails if a port isn't available.
        for port_number in self.WEBSOCKET_PORTS:
            test_socket = socket.socket()
            try:
                test_socket.bind(('localhost', port_number))
                try:
                    port.start_websocket_server()
                    self.fail('should not have been able to start the server while bound to %d' % port_number)
                except http_server_base.ServerError as e:
                    pass
            finally:
                port.stop_websocket_server()
                test_socket.close()

        # Test that calling start() twice fails.
        try:
            port.start_websocket_server()
            self.assertRaises(AssertionError, port.start_websocket_server)
        finally:
            port.stop_websocket_server()

    def integration_test_websocket_server__two_servers(self):
        port = self.make_port()

        # Test that calling start() on two different ports causes the
        # first port to be treated as stale and killed.
        port.start_websocket_server()
        new_port = self.make_port()
        try:
            new_port.start_websocket_server()

            # Check that the first server was killed.
            self.assertFalse(port._executive.check_running_pid(port._websocket_server._pid))

            # Check that there is something running.
            self.assert_servers_are_up('localhost', self.WEBSOCKET_PORTS)

            # Test that calling stop() on a killed server is harmless.
            port.stop_websocket_server()
        finally:
            port.stop_websocket_server()
            new_port.stop_websocket_server()

            # Test that calling stop() twice is harmless.
            new_port.stop_websocket_server()

    def test_test_configuration(self):
        port = self.make_port()
        self.assertTrue(port.test_configuration())

    def test_all_test_configurations(self):
        port = self.make_port()
        self.assertTrue(len(port.all_test_configurations()) > 0)
        self.assertTrue(port.test_configuration() in port.all_test_configurations(), "%s not in %s" % (port.test_configuration(), port.all_test_configurations()))

    def integration_test_http_server__loop(self):
        port = self.make_port()

        i = 0
        while i < 10:
            self.assert_servers_are_down('localhost', self.HTTP_PORTS)
            port.start_http_server()

            # We sleep in between alternating runs to ensure that this
            # test handles both back-to-back starts and stops and
            # starts and stops separated by a delay.
            if i % 2:
                time.sleep(0.1)

            self.assert_servers_are_up('localhost', self.HTTP_PORTS)
            port.stop_http_server()
            if i % 2:
                time.sleep(0.1)

            i += 1

    def test_get_crash_log(self):
        port = self.make_port()
        self.assertEqual(port._get_crash_log(None, None, None, None, newer_than=None),
           (None,
            'crash log for <unknown process name> (pid <unknown>):\n'
            'STDOUT: <empty>\n'
            'STDERR: <empty>\n'))

        self.assertEqual(port._get_crash_log('foo', 1234, 'out bar\nout baz', 'err bar\nerr baz\n', newer_than=None),
            ('err bar\nerr baz\n',
             'crash log for foo (pid 1234):\n'
             'STDOUT: out bar\n'
             'STDOUT: out baz\n'
             'STDERR: err bar\n'
             'STDERR: err baz\n'))

        self.assertEqual(port._get_crash_log('foo', 1234, 'foo\xa6bar', 'foo\xa6bar', newer_than=None),
            ('foo\xa6bar',
             u'crash log for foo (pid 1234):\n'
             u'STDOUT: foo\ufffdbar\n'
             u'STDERR: foo\ufffdbar\n'))

        self.assertEqual(port._get_crash_log('foo', 1234, 'foo\xa6bar', 'foo\xa6bar', newer_than=1.0),
            ('foo\xa6bar',
             u'crash log for foo (pid 1234):\n'
             u'STDOUT: foo\ufffdbar\n'
             u'STDERR: foo\ufffdbar\n'))

    def assert_build_path(self, options, dirs, expected_path):
        port = self.make_port(options=options)
        for directory in dirs:
            port.host.filesystem.maybe_make_directory(directory)
        self.assertEqual(port._build_path(), expected_path)

    def test_expectations_ordering(self):
        port = self.make_port()
        for path in port.expectations_files():
            port._filesystem.write_text_file(path, '')
        ordered_dict = port.expectations_dict()
        self.assertEqual(port.path_to_generic_test_expectations_file(), ordered_dict.keys()[0])
        self.assertEqual(port.path_to_test_expectations_file(), ordered_dict.keys()[port.test_expectations_file_position()])

        options = MockOptions(additional_expectations=['/tmp/foo', '/tmp/bar'])
        port = self.make_port(options=options)
        for path in port.expectations_files():
            port._filesystem.write_text_file(path, '')
        port._filesystem.write_text_file('/tmp/foo', 'foo')
        port._filesystem.write_text_file('/tmp/bar', 'bar')
        ordered_dict = port.expectations_dict()
        self.assertEqual(ordered_dict.keys()[-2:], options.additional_expectations)  # pylint: disable=E1101
        self.assertEqual(ordered_dict.values()[-2:], ['foo', 'bar'])

    def test_path_to_test_expectations_file(self):
        port = TestWebKitPort()
        port._options = MockOptions(webkit_test_runner=False)
        self.assertEqual(port.path_to_test_expectations_file(), '/mock-checkout/LayoutTests/platform/testwebkitport/TestExpectations')

        port = TestWebKitPort()
        port._options = MockOptions(webkit_test_runner=True)
        self.assertEqual(port.path_to_test_expectations_file(), '/mock-checkout/LayoutTests/platform/testwebkitport/TestExpectations')

        port = TestWebKitPort()
        port.host.filesystem.files['/mock-checkout/LayoutTests/platform/testwebkitport/TestExpectations'] = 'some content'
        port._options = MockOptions(webkit_test_runner=False)
        self.assertEqual(port.path_to_test_expectations_file(), '/mock-checkout/LayoutTests/platform/testwebkitport/TestExpectations')

    def test_skipped_layout_tests(self):
        self.assertEqual(TestWebKitPort().skipped_layout_tests(test_list=[]), set(['media']))

    def test_expectations_files(self):
        port = TestWebKitPort()

        def platform_dirs(port):
            return [port.host.filesystem.basename(port.host.filesystem.dirname(f)) for f in port.expectations_files()]

        self.assertEqual(platform_dirs(port), ['LayoutTests', 'testwebkitport'])

        port = TestWebKitPort(port_name="testwebkitport-version")
        self.assertEqual(platform_dirs(port), ['LayoutTests', 'testwebkitport', 'testwebkitport-version'])

        port = TestWebKitPort(port_name="testwebkitport-version-wk2")
        self.assertEqual(platform_dirs(port), ['LayoutTests', 'testwebkitport', 'testwebkitport-version', 'wk2', 'testwebkitport-wk2'])

        port = TestWebKitPort(port_name="testwebkitport-version",
                              options=MockOptions(additional_platform_directory=["internal-testwebkitport"]))
        self.assertEqual(platform_dirs(port), ['LayoutTests', 'testwebkitport', 'testwebkitport-version', 'internal-testwebkitport'])

    def test_root_option(self):
        port = TestWebKitPort()
        port._options = MockOptions(root='/foo')
        if sys.platform.startswith('win'):
            self.assertEqual(port._path_to_driver(), "/foo/DumpRenderTree.exe")
        else:
            self.assertEqual(port._path_to_driver(), "/foo/DumpRenderTree")

    def test_test_expectations(self):
        # Check that we read the expectations file
        host = MockSystemHost()
        host.filesystem.write_text_file('/mock-checkout/LayoutTests/platform/testwebkitport/TestExpectations',
            'BUG_TESTEXPECTATIONS SKIP : fast/html/article-element.html = FAIL\n')
        port = TestWebKitPort(host=host)
        self.assertEqual(''.join(port.expectations_dict().values()), 'BUG_TESTEXPECTATIONS SKIP : fast/html/article-element.html = FAIL\n')

    def test_build_driver(self):
        output = OutputCapture()
        port = TestWebKitPort()
        # Delay setting _executive to avoid logging during construction
        port._executive = MockExecutive(should_log=True)
        port._options = MockOptions(configuration="Release")  # This should not be necessary, but I think TestWebKitPort is actually reading from disk (and thus detects the current configuration).
        expected_logs = "MOCK run_command: ['Tools/Scripts/build-dumprendertree', '--release'], cwd=/mock-checkout, env={'MOCK_ENVIRON_COPY': '1'}\n"
        self.assertTrue(output.assert_outputs(self, port._build_driver, expected_logs=expected_logs))

        # Make sure WebKitTestRunner is used.
        port._options = MockOptions(webkit_test_runner=True, configuration="Release")
        expected_logs = "MOCK run_command: ['Tools/Scripts/build-dumprendertree', '--release'], cwd=/mock-checkout, env={'MOCK_ENVIRON_COPY': '1'}\nMOCK run_command: ['Tools/Scripts/build-webkittestrunner', '--release'], cwd=/mock-checkout, env={'MOCK_ENVIRON_COPY': '1'}\n"
        self.assertTrue(output.assert_outputs(self, port._build_driver, expected_logs=expected_logs))

        # Make sure we show the build log when --verbose is passed, which we simulate by setting the logging level to DEBUG.
        output.set_log_level(logging.DEBUG)
        port._options = MockOptions(configuration="Release")
        expected_logs = """MOCK run_command: ['Tools/Scripts/build-dumprendertree', '--release'], cwd=/mock-checkout, env={'MOCK_ENVIRON_COPY': '1'}
Output of ['Tools/Scripts/build-dumprendertree', '--release']:
MOCK output of child process
"""
        self.assertTrue(output.assert_outputs(self, port._build_driver, expected_logs=expected_logs))
        output.set_log_level(logging.INFO)

        # Make sure that failure to build returns False.
        port._executive = MockExecutive(should_log=True, should_throw=True)
        # Because WK2 currently has to build both webkittestrunner and DRT, if DRT fails, that's the only one it tries.
        expected_logs = """MOCK run_command: ['Tools/Scripts/build-dumprendertree', '--release'], cwd=/mock-checkout, env={'MOCK_ENVIRON_COPY': '1'}
MOCK ScriptError

MOCK output of child process
"""
        self.assertFalse(output.assert_outputs(self, port._build_driver, expected_logs=expected_logs))

    def _assert_config_file_for_platform(self, port, platform, config_file):
        self.assertEqual(port._apache_config_file_name_for_platform(platform), config_file)

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

        port._filesystem = MockFileSystem({'/etc/arch-release': ''})
        self.assertFalse(port._is_redhat_based())
        self.assertTrue(port._is_arch_based())

    def test_apache_config_file_name_for_platform(self):
        port = TestWebKitPort()
        port._apache_version = lambda: '2.2'
        self._assert_config_file_for_platform(port, 'cygwin', 'apache2.2-httpd-win.conf')

        self._assert_config_file_for_platform(port, 'linux2', 'apache2.2-httpd.conf')
        self._assert_config_file_for_platform(port, 'linux3', 'apache2.2-httpd.conf')

        port._is_redhat_based = lambda: True
        port._apache_version = lambda: '2.2'
        self._assert_config_file_for_platform(port, 'linux2', 'fedora-httpd-2.2.conf')

        port = TestWebKitPort()
        port._is_debian_based = lambda: True
        port._apache_version = lambda: '2.2'
        self._assert_config_file_for_platform(port, 'linux2', 'debian-httpd-2.2.conf')

        self._assert_config_file_for_platform(port, 'mac', 'apache2.2-httpd.conf')
        self._assert_config_file_for_platform(port, 'win32', 'win-httpd-2.2-php7.conf')  # WinCairo uses win32. Only AppleWin port uses cygwin.
        self._assert_config_file_for_platform(port, 'barf', 'apache2.2-httpd.conf')

    def test_path_to_apache_config_file(self):
        port = TestWebKitPort()

        saved_environ = os.environ.copy()
        try:
            os.environ['WEBKIT_HTTP_SERVER_CONF_PATH'] = '/path/to/httpd.conf'
            self.assertRaises(IOError, port._path_to_apache_config_file)
            port._filesystem.write_text_file('/existing/httpd.conf', 'Hello, world!')
            os.environ['WEBKIT_HTTP_SERVER_CONF_PATH'] = '/existing/httpd.conf'
            self.assertEqual(port._path_to_apache_config_file(), '/existing/httpd.conf')
        finally:
            os.environ = saved_environ.copy()

        # Mock out _apache_config_file_name_for_platform to ignore the passed sys.platform value.
        port._apache_config_file_name_for_platform = lambda platform: 'httpd.conf'
        self.assertEqual(port._path_to_apache_config_file(), '/mock-checkout/LayoutTests/http/conf/httpd.conf')

        # Check that even if we mock out _apache_config_file_name, the environment variable takes precedence.
        saved_environ = os.environ.copy()
        try:
            os.environ['WEBKIT_HTTP_SERVER_CONF_PATH'] = '/existing/httpd.conf'
            self.assertEqual(port._path_to_apache_config_file(), '/existing/httpd.conf')
        finally:
            os.environ = saved_environ.copy()

    def test_check_build(self):
        port = self.make_port(options=MockOptions(build=True))
        self.build_called = False

        def build_driver_called():
            self.build_called = True
            return True

        port._build_driver = build_driver_called
        port.check_build()
        self.assertTrue(self.build_called)

        port = self.make_port(options=MockOptions(root='/tmp', build=True))
        self.build_called = False
        port._build_driver = build_driver_called
        port.check_build()
        self.assertFalse(self.build_called, None)

        port = self.make_port(options=MockOptions(build=False))
        self.build_called = False
        port._build_driver = build_driver_called
        port.check_build()
        self.assertFalse(self.build_called, None)

    def test_additional_platform_directory(self):
        port = self.make_port(options=MockOptions(additional_platform_directory=['/tmp/foo']))
        self.assertEqual(port.baseline_search_path()[0], '/tmp/foo')

    def test_max_child_processes(self):

        port = self.make_port()
        self.assertEqual(port.max_child_processes(True), 0)
        self.assertEqual(port.max_child_processes(), float('inf'))

    def test_default_upload_configuration(self):
        clear_cached_configuration()
        port = self.make_port()
        configuration = port.configuration_for_upload()
        self.assertEqual(configuration['architecture'], port.architecture())
        self.assertEqual(configuration['is_simulator'], False)
        self.assertEqual(configuration['platform'], port.host.platform.os_name)
        self.assertEqual(configuration['style'], 'release')
        self.assertEqual(configuration['version_name'], port.host.platform.os_version_name())

    def test_debug_upload_configuration(self):
        clear_cached_configuration()
        port = self.make_port(options=MockOptions(configuration='Debug'))
        self.assertEqual(port.configuration_for_upload()['style'], 'debug')

    def test_asan_upload_configuration(self):
        clear_cached_configuration()
        port = self.make_port()
        port.host.filesystem.write_text_file('/mock-build/ASan', 'YES')
        self.assertEqual(port.configuration_for_upload()['style'], 'asan')

    def test_guard_malloc_configuration(self):
        clear_cached_configuration()
        port = self.make_port(options=MockOptions(guard_malloc=True))
        self.assertEqual(port.configuration_for_upload()['style'], 'guard-malloc')

# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2020 Apple Inc. All rights reserved.
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

import optparse
import tempfile
import unittest

from webkitpy.common.system.executive import ScriptError
from webkitpy.common.system import executive_mock
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.common.host_mock import MockHost
from webkitpy.port import Port
from webkitpy.port.config import Config
from webkitpy.port.test import add_unit_tests_to_mock_filesystem, TestPort
from webkitpy.thirdparty.mock import patch

from webkitcorepy import OutputCapture
from webkitscmpy import mocks


def cmp(a, b):
    return (a > b) - (a < b)

class PortTest(unittest.TestCase):
    def setUp(self):
        Config._clear_cache_for_testing()

    def make_port(self, executive=None, with_tests=False, port_name=None, **kwargs):
        host = MockHost(create_stub_repository_files=True)
        if executive:
            host.executive = executive
        if with_tests:
            add_unit_tests_to_mock_filesystem(host.filesystem)
            return TestPort(host, **kwargs)
        return Port(host, port_name or 'baseport', **kwargs)

    def test_default_child_processes(self):
        port = self.make_port()
        self.assertIsNotNone(port.default_child_processes())

    def _file_with_contents(self, contents, encoding="utf-8"):
        new_file = tempfile.NamedTemporaryFile()
        new_file.write(contents.encode(encoding))
        new_file.flush()
        return new_file

    def test_pretty_patch_os_error(self):
        port = self.make_port(executive=executive_mock.MockExecutive2(exception=OSError))
        with OutputCapture():
            self.assertEqual(port.pretty_patch.pretty_patch_text("patch.txt"),
                             port.pretty_patch.pretty_patch_error_html)

            # This tests repeated calls to make sure we cache the result.
            self.assertEqual(port.pretty_patch.pretty_patch_text("patch.txt"),
                             port.pretty_patch.pretty_patch_error_html)

    def test_pretty_patch_script_error(self):
        # FIXME: This is some ugly white-box test hacking ...
        port = self.make_port(executive=executive_mock.MockExecutive2(exception=ScriptError))
        port.pretty_patch.ppatch_available = True
        self.assertEqual(port.pretty_patch.pretty_patch_text("patch.txt"),
                         port.pretty_patch.pretty_patch_error_html)

        # This tests repeated calls to make sure we cache the result.
        self.assertEqual(port.pretty_patch.pretty_patch_text("patch.txt"),
                         port.pretty_patch.pretty_patch_error_html)

    def test_diff_text(self):
        port = self.make_port()
        # Make sure that we don't run into decoding exceptions when the
        # filenames are unicode, with regular or malformed input (expected or
        # actual input is always raw bytes, not unicode).
        port.diff_text('exp', 'act', 'exp.txt', 'act.txt')
        port.diff_text('exp', 'act', u'exp.txt', 'act.txt')
        port.diff_text('exp', 'act', u'a\xac\u1234\u20ac\U00008000', 'act.txt')

        port.diff_text('exp' + chr(255), 'act', 'exp.txt', 'act.txt')
        port.diff_text('exp' + chr(255), 'act', u'exp.txt', 'act.txt')

        # Though expected and actual files should always be read in with no
        # encoding (and be stored as str objects), test unicode inputs just to
        # be safe.
        port.diff_text(u'exp', 'act', 'exp.txt', 'act.txt')
        port.diff_text(
            u'a\xac\u1234\u20ac\U00008000', 'act', 'exp.txt', 'act.txt')

        t1 = "A\n\nB"
        t2 = "A\n\nB\n\n\n"
        t3 = "--- exp.txt\n+++ act.txt\n@@ -1,3 +1,5 @@\n A\n \n-B\n No newline at end of file\n+B\n+\n+\n"
        self.assertEqual(t3, port.diff_text(t1, t2, 'exp.txt', 'act.txt'))

        # And make sure we actually get diff output.
        diff = port.diff_text('foo', 'bar', 'exp.txt', 'act.txt')
        self.assertIn('foo', diff)
        self.assertIn('bar', diff)
        self.assertIn('exp.txt', diff)
        self.assertIn('act.txt', diff)
        self.assertNotIn('nosuchthing', diff)

    def test_setup_test_run(self):
        port = self.make_port()
        # This routine is a no-op. We just test it for coverage.
        port.setup_test_run()

    def test_skipped_perf_tests(self):
        port = self.make_port()

        def add_text_file(dirname, filename, content='some content'):
            dirname = port.host.filesystem.join(port.perf_tests_dir(), dirname)
            port.host.filesystem.maybe_make_directory(dirname)
            port.host.filesystem.write_text_file(port.host.filesystem.join(dirname, filename), content)

        add_text_file('inspector', 'test1.html')
        add_text_file('inspector', 'unsupported_test1.html')
        add_text_file('inspector', 'test2.html')
        add_text_file('inspector/resources', 'resource_file.html')
        add_text_file('unsupported', 'unsupported_test2.html')
        add_text_file('', 'Skipped', '\n'.join(['Layout', '', 'SunSpider', 'Supported/some-test.html', '[ExoticPort] UnskippedTest.html', '[baseport] SkippedTest.html']))
        self.assertEqual(port.skipped_perf_tests(), ['Layout', 'SunSpider', 'Supported/some-test.html', 'SkippedTest.html'])

    def test_get_option__set(self):
        options, args = optparse.OptionParser().parse_args([])
        options.foo = 'bar'
        port = self.make_port(options=options)
        self.assertEqual(port.get_option('foo'), 'bar')

    def test_get_option__unset(self):
        port = self.make_port()
        self.assertIsNone(port.get_option('foo'))

    def test_get_option__default(self):
        port = self.make_port()
        self.assertEqual(port.get_option('foo', 'bar'), 'bar')

    def test_additional_platform_directory(self):
        port = self.make_port(port_name='foo')
        port.default_baseline_search_path = lambda **kwargs: ['LayoutTests/platform/foo']
        test_file = 'fast/test.html'

        # No additional platform directory
        self.assertEqual(
            port.expected_baselines(test_file, '.txt'),
            [(None, 'fast/test-expected.txt')])
        self.assertEqual(port.baseline_path(), 'LayoutTests/platform/foo')

        # Simple additional platform directory
        port._options.additional_platform_directory = ['/tmp/local-baselines']
        port._filesystem.write_text_file('/tmp/local-baselines/fast/test-expected.txt', 'foo')
        self.assertEqual(
            port.expected_baselines(test_file, '.txt'),
            [('/tmp/local-baselines', 'fast/test-expected.txt')])
        self.assertEqual(port.baseline_path(), '/tmp/local-baselines')

        # Multiple additional platform directories
        port._options.additional_platform_directory = ['/foo', '/tmp/local-baselines']
        self.assertEqual(
            port.expected_baselines(test_file, '.txt'),
            [('/tmp/local-baselines', 'fast/test-expected.txt')])
        self.assertEqual(port.baseline_path(), '/foo')

    def test_nonexistant_expectations(self):
        port = self.make_port(port_name='foo')
        port.expectations_files = lambda **kwargs: ['/mock-checkout/LayoutTests/platform/exists/TestExpectations', '/mock-checkout/LayoutTests/platform/nonexistant/TestExpectations']
        port._filesystem.write_text_file('/mock-checkout/LayoutTests/platform/exists/TestExpectations', '')
        self.assertEqual('\n'.join(port.expectations_dict().keys()), '/mock-checkout/LayoutTests/platform/exists/TestExpectations')

    def test_additional_expectations(self):
        port = self.make_port(port_name='foo')
        port.port_name = 'foo'
        port._filesystem.write_text_file('/mock-checkout/LayoutTests/platform/foo/TestExpectations', '')
        port._filesystem.write_text_file(
            '/tmp/additional-expectations-1.txt', 'content1\n')
        port._filesystem.write_text_file(
            '/tmp/additional-expectations-2.txt', 'content2\n')

        self.assertEqual('\n'.join(port.expectations_dict().values()), '')

        port._options.additional_expectations = [
            '/tmp/additional-expectations-1.txt']
        self.assertEqual('\n'.join(port.expectations_dict().values()), '\ncontent1\n')

        port._options.additional_expectations = [
            '/tmp/nonexistent-file', '/tmp/additional-expectations-1.txt']
        self.assertEqual('\n'.join(port.expectations_dict().values()), '\ncontent1\n')

        port._options.additional_expectations = [
            '/tmp/additional-expectations-1.txt', '/tmp/additional-expectations-2.txt']
        self.assertEqual('\n'.join(port.expectations_dict().values()), '\ncontent1\n\ncontent2\n')

    def test_additional_env_var(self):
        port = self.make_port(options=optparse.Values({'additional_env_var': ['FOO=BAR', 'BAR=FOO']}))
        self.assertEqual(port.get_option('additional_env_var'), ['FOO=BAR', 'BAR=FOO'])
        environment = port.setup_environ_for_server()
        self.assertTrue(('FOO' in environment) & ('BAR' in environment))
        self.assertEqual(environment['FOO'], 'BAR')
        self.assertEqual(environment['BAR'], 'FOO')

    def test__merge_sanitizer_options(self):
        merge = Port._merge_sanitizer_options
        self.assertEqual('', merge('', ''))
        self.assertEqual('suppressions=/a.txt', merge('', 'suppressions=/a.txt'))
        # Join with ':', never os.pathsep.
        self.assertEqual('halt_on_error=0:suppressions=/a.txt', merge('halt_on_error=0', 'suppressions=/a.txt'))
        self.assertEqual('suppressions=/mine.txt', merge('suppressions=/mine.txt', 'suppressions=/a.txt'))
        self.assertEqual('halt_on_error=0:suppressions=/mine.txt:log_path=/tmp/t', merge('halt_on_error=0:suppressions=/mine.txt:log_path=/tmp/t', 'suppressions=/a.txt'))
        self.assertEqual('foo_suppressions=/other.txt:suppressions=/a.txt', merge('foo_suppressions=/other.txt', 'suppressions=/a.txt'))
        self.assertEqual('foo_suppressions=/other.txt:suppressions=/a.txt', merge('foo_suppressions=/other.txt:suppressions=/a.txt', 'suppressions=/a.txt'))
        self.assertEqual('log_path=/tmp/a:log_path=/tmp/b', merge('', 'log_path=/tmp/a::log_path=/tmp/b'))
        self.assertEqual('halt_on_error=0:log_path=/tmp/a', merge('halt_on_error=0', 'log_path=/tmp/a:halt_on_error=1'))

    def make_api_test_port(self, additional_env_var=None, tsan=False):
        port = self.make_port(options=optparse.Values({'additional_env_var': additional_env_var or []}))
        port._config.build_directory = lambda configuration: '/mock-build'
        if tsan:
            # A TSan build is identified by the marker file build-webkit --tsan writes.
            port.host.filesystem.write_text_file('/mock-build/TSan', 'YES\n')
        return port

    def make_tsan_api_test_port(self, additional_env_var=None):
        return self.make_api_test_port(additional_env_var, tsan=True)

    def test_environment_for_api_tests_asan_options(self):
        # ASan options apply to every build, TSan or not.
        environment = self.make_api_test_port().environment_for_api_tests()
        self.assertEqual('allocator_may_return_null=1', environment['ASAN_OPTIONS'])
        self.assertEqual(environment['ASAN_OPTIONS'], environment['__XPC_ASAN_OPTIONS'])

    def test_environment_for_api_tests_asan_options_additive(self):
        # ASAN_OPTIONS set with a different key: keep theirs, append ours.
        environment = self.make_api_test_port(['ASAN_OPTIONS=detect_leaks=1']).environment_for_api_tests()
        self.assertEqual('detect_leaks=1:allocator_may_return_null=1', environment['ASAN_OPTIONS'])

    def test_environment_for_api_tests_asan_options_not_clobbered(self):
        # ASAN_OPTIONS already sets our key: leave it alone.
        environment = self.make_api_test_port(['ASAN_OPTIONS=allocator_may_return_null=0']).environment_for_api_tests()
        self.assertEqual('allocator_may_return_null=0', environment['ASAN_OPTIONS'])

    def test_environment_for_api_tests_non_tsan_build(self):
        # Not a TSan build: leave TSAN_OPTIONS alone entirely.
        environment = self.make_api_test_port().environment_for_api_tests()
        self.assertNotIn('TSAN_OPTIONS', environment)
        self.assertNotIn('__XPC_TSAN_OPTIONS', environment)

    def test_environment_for_api_tests_tsan_options_unset(self):
        # TSan build, no TSAN_OPTIONS: set the in-tree suppressions file.
        port = self.make_tsan_api_test_port()
        environment = port.environment_for_api_tests()
        self.assertEqual('second_deadlock_stack=1:suppressions=' + port.path_from_webkit_base('Tools', 'Scripts', 'tsan_suppressions.txt'), environment['TSAN_OPTIONS'])
        # XPC-spawned children only inherit the __XPC_-prefixed twin.
        self.assertEqual(environment['TSAN_OPTIONS'], environment['__XPC_TSAN_OPTIONS'])

    def test_environment_for_api_tests_tsan_options_without_suppressions(self):
        # TSAN_OPTIONS set without suppressions=: append ours, keep theirs.
        # The separator is always ':', even on Windows where os.pathsep is ';'
        # (TSan fails flag parsing and aborts on ';').
        port = self.make_tsan_api_test_port(additional_env_var=['TSAN_OPTIONS=log_path=/tmp/tsan'])
        environment = port.environment_for_api_tests()
        self.assertEqual('log_path=/tmp/tsan:second_deadlock_stack=1:suppressions=' + port.path_from_webkit_base('Tools', 'Scripts', 'tsan_suppressions.txt'), environment['TSAN_OPTIONS'])
        self.assertNotIn(';', environment['TSAN_OPTIONS'])

    def test_environment_for_api_tests_tsan_options_with_suppressions(self):
        # TSAN_OPTIONS already names a suppressions file: leave it alone.
        port = self.make_tsan_api_test_port(additional_env_var=['TSAN_OPTIONS=suppressions=/tmp/mine.txt'])
        environment = port.environment_for_api_tests()
        self.assertEqual('suppressions=/tmp/mine.txt:second_deadlock_stack=1', environment['TSAN_OPTIONS'])

    def test_environment_for_api_tests_asan_options_reach_xpc_services(self):
        environment = self.make_api_test_port(['ASAN_OPTIONS=log_path=/tmp/asan:allocator_may_return_null=0']).environment_for_api_tests()
        self.assertEqual('log_path=/tmp/asan:allocator_may_return_null=0', environment['ASAN_OPTIONS'])
        self.assertEqual('log_path=/tmp/asan:allocator_may_return_null=0', environment['__XPC_ASAN_OPTIONS'])

    def test_environment_for_api_tests_xpc_asan_options_kept(self):
        environment = self.make_api_test_port(['__XPC_ASAN_OPTIONS=log_path=/tmp/xpc']).environment_for_api_tests()
        self.assertEqual('allocator_may_return_null=1', environment['ASAN_OPTIONS'])
        self.assertEqual('log_path=/tmp/xpc:allocator_may_return_null=1', environment['__XPC_ASAN_OPTIONS'])

    def test_environment_for_api_tests_xpc_asan_options_override_asan_options(self):
        environment = self.make_api_test_port([
            'ASAN_OPTIONS=log_path=/tmp/asan:detect_stack_use_after_return=1',
            '__XPC_ASAN_OPTIONS=allocator_may_return_null=0:log_path=/tmp/xpc',
        ]).environment_for_api_tests()
        self.assertEqual('log_path=/tmp/asan:detect_stack_use_after_return=1:allocator_may_return_null=1', environment['ASAN_OPTIONS'])
        self.assertEqual('allocator_may_return_null=0:log_path=/tmp/xpc:detect_stack_use_after_return=1', environment['__XPC_ASAN_OPTIONS'])

    def test_environment_for_api_tests_tsan_options_reach_xpc_services_non_tsan_build(self):
        environment = self.make_api_test_port(['TSAN_OPTIONS=halt_on_error=1:log_path=/tmp/tsan']).environment_for_api_tests()
        self.assertEqual('halt_on_error=1:log_path=/tmp/tsan', environment['TSAN_OPTIONS'])
        self.assertEqual('halt_on_error=1:log_path=/tmp/tsan', environment['__XPC_TSAN_OPTIONS'])

    def test_environment_for_api_tests_tsan_options_reach_xpc_services(self):
        port = self.make_tsan_api_test_port(additional_env_var=['TSAN_OPTIONS=log_path=/tmp/tsan:second_deadlock_stack=0'])
        environment = port.environment_for_api_tests()
        expected = 'log_path=/tmp/tsan:second_deadlock_stack=0:suppressions=' + port.path_from_webkit_base('Tools', 'Scripts', 'tsan_suppressions.txt')
        self.assertEqual(expected, environment['TSAN_OPTIONS'])
        self.assertEqual(expected, environment['__XPC_TSAN_OPTIONS'])

    def test_environment_for_api_tests_xpc_tsan_options_kept(self):
        port = self.make_tsan_api_test_port(additional_env_var=['__XPC_TSAN_OPTIONS=halt_on_error=1'])
        environment = port.environment_for_api_tests()
        suppressions = 'suppressions=' + port.path_from_webkit_base('Tools', 'Scripts', 'tsan_suppressions.txt')
        self.assertEqual('second_deadlock_stack=1:' + suppressions, environment['TSAN_OPTIONS'])
        self.assertEqual('halt_on_error=1:second_deadlock_stack=1:' + suppressions, environment['__XPC_TSAN_OPTIONS'])

    def test_environment_for_api_tests_xpc_tsan_options_override_tsan_options(self):
        environment = self.make_tsan_api_test_port(additional_env_var=[
            'TSAN_OPTIONS=suppressions=/tmp/mine.txt:halt_on_error=0',
            '__XPC_TSAN_OPTIONS=suppressions=/tmp/xpc.txt:second_deadlock_stack=0',
        ]).environment_for_api_tests()
        self.assertEqual('suppressions=/tmp/mine.txt:halt_on_error=0:second_deadlock_stack=1', environment['TSAN_OPTIONS'])
        self.assertEqual('suppressions=/tmp/xpc.txt:second_deadlock_stack=0:halt_on_error=0', environment['__XPC_TSAN_OPTIONS'])

    def test_environment_for_api_tests_lsan_and_ubsan_options_reach_xpc_services(self):
        environment = self.make_api_test_port([
            'LSAN_OPTIONS=log_path=/tmp/lsan:report_objects=1',
            '__XPC_LSAN_OPTIONS=log_path=/tmp/xpc',
            'UBSAN_OPTIONS=print_stacktrace=1',
        ]).environment_for_api_tests()
        self.assertEqual('log_path=/tmp/xpc:report_objects=1', environment['__XPC_LSAN_OPTIONS'])
        self.assertEqual('print_stacktrace=1', environment['__XPC_UBSAN_OPTIONS'])

    def test_environment_for_api_tests_sanitizer_options_split_on_every_separator(self):
        environment = self.make_tsan_api_test_port([
            'ASAN_OPTIONS=detect_leaks=1,allocator_may_return_null=0',
            'TSAN_OPTIONS=halt_on_error=1 second_deadlock_stack=0\tsuppressions=/tmp/mine.txt',
        ]).environment_for_api_tests()
        self.assertEqual('detect_leaks=1:allocator_may_return_null=0', environment['ASAN_OPTIONS'])
        self.assertEqual('detect_leaks=1:allocator_may_return_null=0', environment['__XPC_ASAN_OPTIONS'])
        self.assertEqual('halt_on_error=1:second_deadlock_stack=0:suppressions=/tmp/mine.txt', environment['TSAN_OPTIONS'])
        self.assertEqual('halt_on_error=1:second_deadlock_stack=0:suppressions=/tmp/mine.txt', environment['__XPC_TSAN_OPTIONS'])

    def test_environment_for_api_tests_sanitizer_options_keep_quoted_values(self):
        environment = self.make_api_test_port([
            'ASAN_OPTIONS=log_path="/tmp/a:b"',
            "__XPC_ASAN_OPTIONS=log_path='/tmp/x y'",
        ]).environment_for_api_tests()
        self.assertEqual('log_path="/tmp/a:b":allocator_may_return_null=1', environment['ASAN_OPTIONS'])
        self.assertEqual("log_path='/tmp/x y':allocator_may_return_null=1", environment['__XPC_ASAN_OPTIONS'])

    def test_environment_for_api_tests_tsan_suppressions_path_with_separator(self):
        port = self.make_tsan_api_test_port()
        port.path_from_webkit_base = lambda *comps: '/Web Kit/' + '/'.join(comps)
        environment = port.environment_for_api_tests()
        self.assertEqual('second_deadlock_stack=1:suppressions="/Web Kit/Tools/Scripts/tsan_suppressions.txt"', environment['TSAN_OPTIONS'])

    def test_environment_for_api_tests_sanitizer_options_ignore_shell_environment(self):
        with patch('os.environ', {'ASAN_OPTIONS': 'log_path=/tmp/shell', '__XPC_TSAN_OPTIONS': 'halt_on_error=1'}):
            environment = self.make_api_test_port([]).environment_for_api_tests()
        self.assertEqual('allocator_may_return_null=1', environment['ASAN_OPTIONS'])
        self.assertEqual('allocator_may_return_null=1', environment['__XPC_ASAN_OPTIONS'])
        self.assertNotIn('__XPC_TSAN_OPTIONS', environment)

    def test_warn_about_ignored_sanitizer_options_asan_options(self):
        with patch('os.environ', {'ASAN_OPTIONS': 'log_path=/tmp/shell'}), OutputCapture() as captured:
            self.make_api_test_port([]).warn_about_ignored_sanitizer_options()
        self.assertEqual('ASAN_OPTIONS is set in the environment but is ignored; pass it with --additional-env-var=ASAN_OPTIONS=... instead.\n', captured.root.log.getvalue())

    def test_warn_about_ignored_sanitizer_options_every_variable(self):
        names = [
            'ASAN_OPTIONS', '__XPC_ASAN_OPTIONS',
            'LSAN_OPTIONS', '__XPC_LSAN_OPTIONS',
            'TSAN_OPTIONS', '__XPC_TSAN_OPTIONS',
            'UBSAN_OPTIONS', '__XPC_UBSAN_OPTIONS',
        ]
        with patch('os.environ', {name: 'halt_on_error=1' for name in names}), OutputCapture() as captured:
            self.make_api_test_port([]).warn_about_ignored_sanitizer_options()
        self.assertEqual(
            ['{} is set in the environment but is ignored; pass it with --additional-env-var={}=... instead.'.format(name, name) for name in names],
            captured.root.log.getvalue().splitlines())

    def test_warn_about_ignored_sanitizer_options_unset_or_empty(self):
        for environ in [{}, {'ASAN_OPTIONS': '', '__XPC_TSAN_OPTIONS': ''}, {'PATH': '/usr/bin', 'MY_ASAN_OPTIONS': 'log_path=/tmp/x'}]:
            with patch('os.environ', environ), OutputCapture() as captured:
                self.make_api_test_port([]).warn_about_ignored_sanitizer_options()
            self.assertEqual('', captured.root.log.getvalue(), environ)

    def test_warn_about_ignored_sanitizer_options_passed_with_additional_env_var(self):
        with patch('os.environ', {'ASAN_OPTIONS': 'log_path=/tmp/shell', '__XPC_ASAN_OPTIONS': 'log_path=/tmp/shell', 'TSAN_OPTIONS': 'halt_on_error=1'}), OutputCapture() as captured:
            self.make_api_test_port(['ASAN_OPTIONS=log_path=/tmp/flag', 'TSAN_OPTIONS=halt_on_error=1']).warn_about_ignored_sanitizer_options()
        self.assertEqual('__XPC_ASAN_OPTIONS is set in the environment but is ignored; pass it with --additional-env-var=__XPC_ASAN_OPTIONS=... instead.\n', captured.root.log.getvalue())

    def test_uses_test_expectations_file(self):
        port = self.make_port(port_name='foo')
        port.port_name = 'foo'
        self.assertFalse(port.uses_test_expectations_file())
        port._filesystem = MockFileSystem({'/mock-checkout/LayoutTests/platform/foo/TestExpectations': ''})
        self.assertTrue(port.uses_test_expectations_file())

    def test_operating_system(self):
        self.assertEqual('mac', self.make_port().operating_system())

    def test_http_server_supports_ipv6(self):
        port = self.make_port()
        self.assertTrue(port.http_server_supports_ipv6())
        port.host.platform.os_name = 'cygwin'
        self.assertFalse(port.http_server_supports_ipv6())
        port.host.platform.os_name = 'win'
        self.assertTrue(port.http_server_supports_ipv6())

    def test_check_httpd_success(self):
        port = self.make_port(executive=MockExecutive2())
        port._path_to_apache = lambda: '/usr/sbin/httpd'
        with OutputCapture() as captured:
            self.assertTrue(port.check_httpd())
        self.assertEqual('', captured.root.log.getvalue())

    def test_httpd_returns_error_code(self):
        port = self.make_port(executive=MockExecutive2(exit_code=1))
        port._path_to_apache = lambda: '/usr/sbin/httpd'
        with OutputCapture() as captured:
            self.assertFalse(port.check_httpd())
        self.assertEqual('httpd seems broken. Cannot run http tests.\n', captured.root.log.getvalue())

    def test_test_exists(self):
        port = self.make_port(with_tests=True)
        self.assertTrue(port.test_exists('passes'))
        self.assertTrue(port.test_exists('passes/text.html'))
        self.assertFalse(port.test_exists('passes/does_not_exist.html'))
        self.assertTrue(port.test_exists('variant/variant.any.html?1-100'))
        self.assertTrue(port.test_exists('variant/variant.any.html?val=2.3'))

    def test_test_isfile(self):
        port = self.make_port(with_tests=True)
        self.assertFalse(port.test_isfile('passes'))
        self.assertTrue(port.test_isfile('passes/text.html'))
        self.assertFalse(port.test_isfile('passes/does_not_exist.html'))

    def test_test_isdir(self):
        port = self.make_port(with_tests=True)
        self.assertTrue(port.test_isdir('passes'))
        self.assertFalse(port.test_isdir('passes/text.html'))
        self.assertFalse(port.test_isdir('passes/does_not_exist.html'))
        self.assertFalse(port.test_isdir('passes/does_not_exist/'))

    def test_build_path(self):
        port = self.make_port(
            executive=MockExecutive2(output='/default-build-path/Debug'),
            options=optparse.Values({'build_directory': '/my-build-directory/'}),
        )
        self.assertEqual(port._build_path(), '/my-build-directory/Debug')

        Config._clear_cache_for_testing()
        port = self.make_port(
            executive=MockExecutive2(output='/default-build-path/Debug-embedded-port'),
            options=optparse.Values({'build_directory': '/my-build-directory/'}),
        )
        self.assertEqual(port._build_path(), '/my-build-directory/Debug-embedded-port')

    def test_arguments_for_configuration(self):
        port = self.make_port(options=optparse.Values({'configuration': 'Release'}))
        self.assertEqual(port._arguments_for_configuration(), ['--release'])

        port = self.make_port(options=optparse.Values({'configuration': 'Debug', 'use_cmake': True, 'asan': True}))
        self.assertEqual(port._arguments_for_configuration(), ['--debug', '--cmake', '--asan'])

        port = self.make_port(options=optparse.Values({'configuration': 'Release', 'use_xcode': True}))
        self.assertEqual(port._arguments_for_configuration(), ['--release', '--xcode'])

    def test_run_browser_forwards_build_selection(self):
        for run_browser in (Port.run_minibrowser, Port.run_swiftbrowser):
            commands = []

            def run_command(args, **kwargs):
                commands.append(args)
                return 0

            port = self.make_port(options=optparse.Values({'configuration': 'Debug', 'platform': 'mac', 'use_xcode': True}))
            port._executive.run_command = run_command
            run_browser(port, ['https://webkit.org'])
            self.assertEqual(commands[-1][1:], ['https://webkit.org', '--debug', '--xcode', '--mac'])

            port = self.make_port(options=optparse.Values({'configuration': 'Release', 'platform': 'mac', 'use_cmake': True, 'asan': True}))
            port._executive.run_command = run_command
            run_browser(port, [])
            self.assertEqual(commands[-1][1:], ['--release', '--cmake', '--asan', '--mac'])

    def test_jhbuild_wrapper(self):
        port = self.make_port(port_name='foo')
        port.port_name = 'foo'
        jhbuild_path = port.path_from_webkit_base('WebKitBuild', 'Dependencies%s' % port.port_name.upper())
        self.assertFalse(port._filesystem.isdir(jhbuild_path))
        self.assertFalse(port._should_use_jhbuild())
        port._filesystem.maybe_make_directory(jhbuild_path)
        self.assertTrue(port._filesystem.isdir(jhbuild_path))
        # false unless WEBKIT_JHBUILD=1 in env
        self.assertFalse(port._should_use_jhbuild())
        with patch('os.environ', {'WEBKIT_JHBUILD': '1'}):
            self.assertTrue(port._should_use_jhbuild())

    def test_commits_for_upload(self):
        with mocks.local.Svn(path='/'), mocks.local.Git():
            port = self.make_port(port_name='foo')
            self.assertEqual([{'repository_id': 'webkit', 'id': '6', 'branch': 'trunk'}], port.commits_for_upload())

    def test_commits_for_upload_git_svn(self):
        with mocks.local.Svn(), mocks.local.Git(path='/', git_svn=True), OutputCapture():
            port = self.make_port(port_name='foo')
            self.assertEqual([{
                'repository_id': 'webkit',
                'revision': 9,
                'hash': 'd8bce26fa65c6fc8f39c17927abb77f69fab82fc',
                'identifier': '5@main',
                'branch': 'main',
                'author': {'emails': ['jbedard@apple.com'], 'name': 'Jonathan Bedard'},
                'message': 'Patch Series\ngit-svn-id: https://svn.example.org/repository//trunk@9 268f45cc-cd09-0410-ab3c-d52691b4dbfc',
                'timestamp': 1601668000,
                'order': 1,
            }], port.commits_for_upload())


class NaturalCompareTest(unittest.TestCase):
    def setUp(self):
        self._port = TestPort(MockSystemHost())

    def assert_cmp(self, x, y, result):
        self.assertEqual(cmp(self._port._natural_sort_key(x), self._port._natural_sort_key(y)), result)

    def test_natural_compare(self):
        self.assert_cmp('a', 'a', 0)
        self.assert_cmp('ab', 'a', 1)
        self.assert_cmp('a', 'ab', -1)
        self.assert_cmp('', '', 0)
        self.assert_cmp('', 'ab', -1)
        self.assert_cmp('01', '1', -1)
        self.assert_cmp('001', '1', -1)
        self.assert_cmp('001', '01', -1)
        self.assert_cmp('1', '2', -1)
        self.assert_cmp('2', '1', 1)
        self.assert_cmp('1', '10', -1)
        self.assert_cmp('2', '10', -1)
        self.assert_cmp('foo_1.html', 'foo_2.html', -1)
        self.assert_cmp('foo_1.1.html', 'foo_2.html', -1)
        self.assert_cmp('foo_1.html', 'foo_10.html', -1)
        self.assert_cmp('foo_2.html', 'foo_10.html', -1)
        self.assert_cmp('foo_23.html', 'foo_10.html', 1)
        self.assert_cmp('foo_23.html', 'foo_100.html', -1)


class KeyCompareTest(unittest.TestCase):
    def setUp(self):
        self._port = TestPort(MockSystemHost())

    def assert_cmp(self, x, y, result):
        self.assertEqual(cmp(self._port.test_key(x), self._port.test_key(y)), result)

    def test_test_key(self):
        self.assert_cmp('/a', '/a', 0)
        self.assert_cmp('/a', '/b', -1)
        self.assert_cmp('/a', '/a2', -1)
        self.assert_cmp('/a2', '/a10', -1)
        self.assert_cmp('/a2/foo', '/a10/foo', -1)
        self.assert_cmp('/a/foo11', '/a/foo2', 1)
        self.assert_cmp('/a/foo1', '/a/foo01', 1)
        self.assert_cmp('/a/foo01', '/a/foo001', 1)
        self.assert_cmp('/ab', '/a/a/b', -1)
        self.assert_cmp('/a/a/b', '/ab', 1)
        self.assert_cmp('/foo-bar/baz', '/foo/baz', -1)
        self.assert_cmp('/foo!bar/baz', '/foo/bar/baz', -1)
        self.assert_cmp('/foo-bar/baz', '/foo/bar/baz', -1)
        self.assert_cmp('/foo_bar/baz', '/foo/bar/baz', 1)

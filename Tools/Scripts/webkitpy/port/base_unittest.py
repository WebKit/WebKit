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

import optparse
import tempfile
import unittest

from webkitpy.common.system.executive import ScriptError
from webkitpy.common.system import executive_mock
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.common.system.systemhost_mock import MockSystemHost

from webkitpy.port import Port
from webkitpy.port.test import add_unit_tests_to_mock_filesystem, TestPort


class PortTest(unittest.TestCase):
    def make_port(self, executive=None, with_tests=False, port_name=None, **kwargs):
        host = MockSystemHost()
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
        oc = OutputCapture()
        oc.capture_output()
        self.assertEqual(port.pretty_patch.pretty_patch_text("patch.txt"),
                         port.pretty_patch.pretty_patch_error_html)

        # This tests repeated calls to make sure we cache the result.
        self.assertEqual(port.pretty_patch.pretty_patch_text("patch.txt"),
                         port.pretty_patch.pretty_patch_error_html)
        oc.restore_output()

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
        t3 = "--- exp.txt\n+++ act.txt\n@@ -1,3 +1,5 @@\n A\n \n-B\n\ No newline at end of file\n+B\n+\n+\n"
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

    def test_test_dirs(self):
        port = self.make_port()
        port.host.filesystem.write_text_file(port.layout_tests_dir() + '/canvas/test', '')
        port.host.filesystem.write_text_file(port.layout_tests_dir() + '/css2.1/test', '')
        dirs = port.test_dirs()
        self.assertIn('canvas', dirs)
        self.assertIn('css2.1', dirs)

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

    def test_uses_test_expectations_file(self):
        port = self.make_port(port_name='foo')
        port.port_name = 'foo'
        port.path_to_test_expectations_file = lambda: '/mock-results/TestExpectations'
        self.assertFalse(port.uses_test_expectations_file())
        port._filesystem = MockFileSystem({'/mock-results/TestExpectations': ''})
        self.assertTrue(port.uses_test_expectations_file())

    def test_find_no_paths_specified(self):
        port = self.make_port(with_tests=True)
        tests = port.tests([])
        self.assertNotEqual(len(tests), 0)

    def test_find_one_test(self):
        port = self.make_port(with_tests=True)
        tests = port.tests(['failures/expected/image.html'])
        self.assertEqual(len(tests), 1)

    def test_find_glob(self):
        port = self.make_port(with_tests=True)
        tests = port.tests(['failures/expected/im*'])
        self.assertEqual(len(tests), 2)

    def test_find_with_skipped_directories(self):
        port = self.make_port(with_tests=True)
        tests = port.tests(['userscripts'])
        self.assertNotIn('userscripts/resources/iframe.html', tests)

    def test_find_with_skipped_directories_2(self):
        port = self.make_port(with_tests=True)
        tests = port.tests(['userscripts/resources'])
        self.assertEqual(tests, [])

    def test_is_test_file(self):
        port = self.make_port()
        self.assertTrue(port._is_test_file(port.host.filesystem, '', 'foo.html'))
        self.assertTrue(port._is_test_file(port.host.filesystem, '', 'foo.shtml'))
        self.assertTrue(port._is_test_file(port.host.filesystem, '', 'foo.svg'))
        self.assertTrue(port._is_test_file(port.host.filesystem, '', 'test-ref-test.html'))
        self.assertFalse(port._is_test_file(port.host.filesystem, '', 'foo.png'))
        self.assertFalse(port._is_test_file(port.host.filesystem, '', 'foo-expected.html'))
        self.assertFalse(port._is_test_file(port.host.filesystem, '', 'foo-expected.svg'))
        self.assertFalse(port._is_test_file(port.host.filesystem, '', 'foo-expected.xht'))
        self.assertFalse(port._is_test_file(port.host.filesystem, '', 'foo-expected-mismatch.html'))
        self.assertFalse(port._is_test_file(port.host.filesystem, '', 'foo-expected-mismatch.svg'))
        self.assertFalse(port._is_test_file(port.host.filesystem, '', 'foo-expected-mismatch.xhtml'))
        self.assertFalse(port._is_test_file(port.host.filesystem, '', 'foo-ref.html'))
        self.assertFalse(port._is_test_file(port.host.filesystem, '', 'foo-notref.html'))
        self.assertFalse(port._is_test_file(port.host.filesystem, '', 'foo-notref.xht'))
        self.assertFalse(port._is_test_file(port.host.filesystem, '', 'foo-ref.xhtml'))
        self.assertFalse(port._is_test_file(port.host.filesystem, '', 'ref-foo.html'))
        self.assertFalse(port._is_test_file(port.host.filesystem, '', 'notref-foo.xhr'))

    def test_is_reference_html_file(self):
        filesystem = MockFileSystem()
        self.assertTrue(Port.is_reference_html_file(filesystem, '', 'foo-expected.html'))
        self.assertTrue(Port.is_reference_html_file(filesystem, '', 'foo-expected-mismatch.xml'))
        self.assertTrue(Port.is_reference_html_file(filesystem, '', 'foo-ref.xhtml'))
        self.assertTrue(Port.is_reference_html_file(filesystem, '', 'foo-notref.svg'))
        self.assertFalse(Port.is_reference_html_file(filesystem, '', 'foo.html'))
        self.assertFalse(Port.is_reference_html_file(filesystem, '', 'foo-expected.txt'))
        self.assertFalse(Port.is_reference_html_file(filesystem, '', 'foo-expected.shtml'))
        self.assertFalse(Port.is_reference_html_file(filesystem, '', 'foo-expected.php'))
        self.assertFalse(Port.is_reference_html_file(filesystem, '', 'foo-expected.mht'))

    def test_parse_reftest_list(self):
        port = self.make_port(with_tests=True)
        port.host.filesystem.files['bar/reftest.list'] = "\n".join(["== test.html test-ref.html",
        "",
        "# some comment",
        "!= test-2.html test-notref.html # more comments",
        "== test-3.html test-ref.html",
        "== test-3.html test-ref2.html",
        "!= test-3.html test-notref.html"])

        reftest_list = Port._parse_reftest_list(port.host.filesystem, 'bar')
        self.assertEqual(reftest_list, {'bar/test.html': [('==', 'bar/test-ref.html')],
            'bar/test-2.html': [('!=', 'bar/test-notref.html')],
            'bar/test-3.html': [('==', 'bar/test-ref.html'), ('==', 'bar/test-ref2.html'), ('!=', 'bar/test-notref.html')]})

    def test_reference_files(self):
        port = self.make_port(with_tests=True)
        self.assertEqual(port.reference_files('passes/svgreftest.svg'), [('==', port.layout_tests_dir() + '/passes/svgreftest-expected.svg')])
        self.assertEqual(port.reference_files('passes/xhtreftest.svg'), [('==', port.layout_tests_dir() + '/passes/xhtreftest-expected.html')])
        self.assertEqual(port.reference_files('passes/phpreftest.php'), [('!=', port.layout_tests_dir() + '/passes/phpreftest-expected-mismatch.svg')])

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
        capture = OutputCapture()
        capture.capture_output()
        self.assertTrue(port.check_httpd())
        _, _, logs = capture.restore_output()
        self.assertEqual('', logs)

    def test_httpd_returns_error_code(self):
        port = self.make_port(executive=MockExecutive2(exit_code=1))
        port._path_to_apache = lambda: '/usr/sbin/httpd'
        capture = OutputCapture()
        capture.capture_output()
        self.assertFalse(port.check_httpd())
        _, _, logs = capture.restore_output()
        self.assertEqual('httpd seems broken. Cannot run http tests.\n', logs)

    def test_test_exists(self):
        port = self.make_port(with_tests=True)
        self.assertTrue(port.test_exists('passes'))
        self.assertTrue(port.test_exists('passes/text.html'))
        self.assertFalse(port.test_exists('passes/does_not_exist.html'))

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

    def test_tests(self):
        port = self.make_port(with_tests=True)
        tests = port.tests([])
        self.assertIn('passes/text.html', tests)

        tests = port.tests(['passes'])
        self.assertIn('passes/text.html', tests)

    def test_build_path(self):
        port = self.make_port(
            executive=MockExecutive2(output='/default-build-path/Debug'),
            options=optparse.Values({'build_directory': '/my-build-directory/'}),
        )
        self.assertEqual(port._build_path(), '/my-build-directory/Debug')

        port = self.make_port(
            executive=MockExecutive2(output='/default-build-path/Debug-embedded-port'),
            options=optparse.Values({'build_directory': '/my-build-directory/'}),
        )
        self.assertEqual(port._build_path(), '/my-build-directory/Debug-embedded-port')

    def test_is_w3c_resource_file(self):
        port = self.make_port()
        port.host.filesystem.write_text_file(port.layout_tests_dir() + "/imported/w3c/resources/resource-files.json", """
{"directories": [
"web-platform-tests/common",
"web-platform-tests/dom/nodes/Document-createElement-namespace-tests",
"web-platform-tests/fonts",
"web-platform-tests/html/browsers/browsing-the-web/navigating-across-documents/source/support",
"web-platform-tests/html/browsers/browsing-the-web/unloading-documents/support",
"web-platform-tests/html/browsers/history/the-history-interface/non-automated",
"web-platform-tests/html/browsers/history/the-location-interface/non-automated",
"web-platform-tests/images",
"web-platform-tests/service-workers",
"web-platform-tests/tools"
], "files": [
"web-platform-tests/XMLHttpRequest/xmlhttprequest-sync-block-defer-scripts-subframe.html",
"web-platform-tests/XMLHttpRequest/xmlhttprequest-sync-not-hang-scriptloader-subframe.html"
]}""")
        self.assertFalse(port.is_w3c_resource_file(port.host.filesystem, port.layout_tests_dir() + "/imported/w3", "resource_file.html"))
        self.assertFalse(port.is_w3c_resource_file(port.host.filesystem, port.layout_tests_dir() + "/imported/w3c", "resource_file.html"))
        self.assertFalse(port.is_w3c_resource_file(port.host.filesystem, port.layout_tests_dir() + "/imported/w3c/web-platform-tests/XMLHttpRequest", "xmlhttprequest-sync-block-defer-scripts-subframe.html.html"))
        self.assertTrue(port.is_w3c_resource_file(port.host.filesystem, port.layout_tests_dir() + "/imported/w3c/web-platform-tests/XMLHttpRequest", "xmlhttprequest-sync-block-defer-scripts-subframe.html"))
        self.assertTrue(port.is_w3c_resource_file(port.host.filesystem, port.layout_tests_dir() + "/imported/w3c/web-platform-tests/dom/nodes/Document-createElement-namespace-tests", "test.html"))

    def test_jhbuild_wrapper(self):
        port = self.make_port(port_name='foo')
        port.port_name = 'foo'
        jhbuild_path = port.path_from_webkit_base('WebKitBuild', 'Dependencies%s' % port.port_name.upper())
        self.assertFalse(port._filesystem.isdir(jhbuild_path))
        self.assertFalse(port._should_use_jhbuild())
        port._filesystem.maybe_make_directory(jhbuild_path)
        self.assertTrue(port._filesystem.isdir(jhbuild_path))
        self.assertTrue(port._should_use_jhbuild())

    def test_ref_tests_platform_directory(self):
        port = self.make_port(port_name='foo')
        port.default_baseline_search_path = lambda **kwargs: ['/mock-checkout/LayoutTests/platform/foo']
        port._filesystem.write_text_file('/mock-checkout/LayoutTests/fast/ref-expected.html', 'foo')

        # No platform directory
        self.assertEqual(
            [('==', '/mock-checkout/LayoutTests/fast/ref-expected.html')],
            port.reference_files('fast/ref.html'),
        )

        port._filesystem.write_text_file('/mock-checkout/LayoutTests/platform/foo/fast/ref-expected-mismatch.html', 'foo-plat')
        self.assertEqual(
            [('!=', '/mock-checkout/LayoutTests/platform/foo/fast/ref-expected-mismatch.html')],
            port.reference_files('fast/ref.html'),
        )


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
        self.assert_cmp('/a2', '/a10', -1)
        self.assert_cmp('/a2/foo', '/a10/foo', -1)
        self.assert_cmp('/a/foo11', '/a/foo2', 1)
        self.assert_cmp('/ab', '/a/a/b', -1)
        self.assert_cmp('/a/a/b', '/ab', 1)
        self.assert_cmp('/foo-bar/baz', '/foo/baz', -1)

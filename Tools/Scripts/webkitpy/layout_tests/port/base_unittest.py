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
import sys
import tempfile
import unittest

from webkitpy.common.system.executive import Executive, ScriptError
from webkitpy.common.system import executive_mock
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.common.system.path import abspath_to_uri
from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.mocktool import MockOptions
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.host_mock import MockHost

from webkitpy.layout_tests.port import Port, Driver, DriverOutput
from webkitpy.layout_tests.port.test import TestPort

import config
import config_mock


class PortTest(unittest.TestCase):
    def make_port(self, host=None, **kwargs):
        host = host or MockHost()
        return Port(host, **kwargs)

    def test_default_child_processes(self):
        port = self.make_port()
        # Even though the MockPlatformInfo shows 1GB free memory (enough for 5 DRT instances)
        # we're still limited by the 2 mock cores we have:
        self.assertEqual(port.default_child_processes(), 2)
        bytes_for_drt = 200 * 1024 * 1024

        port.host.platform.free_bytes_memory = lambda: bytes_for_drt
        expected_stdout = "This machine could support 2 child processes, but only has enough memory for 1.\n"
        child_processes = OutputCapture().assert_outputs(self, port.default_child_processes, (), expected_stdout=expected_stdout)
        self.assertEqual(child_processes, 1)

        # Make sure that we always use one process, even if we don't have the memory for it.
        port.host.platform.free_bytes_memory = lambda: bytes_for_drt - 1
        expected_stdout = "This machine could support 2 child processes, but only has enough memory for 1.\n"
        child_processes = OutputCapture().assert_outputs(self, port.default_child_processes, (), expected_stdout=expected_stdout)
        self.assertEqual(child_processes, 1)

    def test_format_wdiff_output_as_html(self):
        output = "OUTPUT %s %s %s" % (Port._WDIFF_DEL, Port._WDIFF_ADD, Port._WDIFF_END)
        html = self.make_port()._format_wdiff_output_as_html(output)
        expected_html = "<head><style>.del { background: #faa; } .add { background: #afa; }</style></head><pre>OUTPUT <span class=del> <span class=add> </span></pre>"
        self.assertEqual(html, expected_html)

    def test_wdiff_command(self):
        port = self.make_port()
        port._path_to_wdiff = lambda: "/path/to/wdiff"
        command = port._wdiff_command("/actual/path", "/expected/path")
        expected_command = [
            "/path/to/wdiff",
            "--start-delete=##WDIFF_DEL##",
            "--end-delete=##WDIFF_END##",
            "--start-insert=##WDIFF_ADD##",
            "--end-insert=##WDIFF_END##",
            "/actual/path",
            "/expected/path",
        ]
        self.assertEqual(command, expected_command)

    def _file_with_contents(self, contents, encoding="utf-8"):
        new_file = tempfile.NamedTemporaryFile()
        new_file.write(contents.encode(encoding))
        new_file.flush()
        return new_file

    def test_pretty_patch_os_error(self):
        port = self.make_port(executive=executive_mock.MockExecutive2(exception=OSError))
        oc = OutputCapture()
        oc.capture_output()
        self.assertEqual(port.pretty_patch_text("patch.txt"),
                         port._pretty_patch_error_html)

        # This tests repeated calls to make sure we cache the result.
        self.assertEqual(port.pretty_patch_text("patch.txt"),
                         port._pretty_patch_error_html)
        oc.restore_output()

    def test_pretty_patch_script_error(self):
        # FIXME: This is some ugly white-box test hacking ...
        port = self.make_port(executive=executive_mock.MockExecutive2(exception=ScriptError))
        port._pretty_patch_available = True
        self.assertEqual(port.pretty_patch_text("patch.txt"),
                         port._pretty_patch_error_html)

        # This tests repeated calls to make sure we cache the result.
        self.assertEqual(port.pretty_patch_text("patch.txt"),
                         port._pretty_patch_error_html)

    def integration_test_run_wdiff(self):
        executive = Executive()
        # This may fail on some systems.  We could ask the port
        # object for the wdiff path, but since we don't know what
        # port object to use, this is sufficient for now.
        try:
            wdiff_path = executive.run_command(["which", "wdiff"]).rstrip()
        except Exception, e:
            wdiff_path = None

        port = self.make_port(executive=executive)
        port._path_to_wdiff = lambda: wdiff_path

        if wdiff_path:
            # "with tempfile.NamedTemporaryFile() as actual" does not seem to work in Python 2.5
            actual = self._file_with_contents(u"foo")
            expected = self._file_with_contents(u"bar")
            wdiff = port._run_wdiff(actual.name, expected.name)
            expected_wdiff = "<head><style>.del { background: #faa; } .add { background: #afa; }</style></head><pre><span class=del>foo</span><span class=add>bar</span></pre>"
            self.assertEqual(wdiff, expected_wdiff)
            # Running the full wdiff_text method should give the same result.
            port._wdiff_available = True  # In case it's somehow already disabled.
            wdiff = port.wdiff_text(actual.name, expected.name)
            self.assertEqual(wdiff, expected_wdiff)
            # wdiff should still be available after running wdiff_text with a valid diff.
            self.assertTrue(port._wdiff_available)
            actual.close()
            expected.close()

            # Bogus paths should raise a script error.
            self.assertRaises(ScriptError, port._run_wdiff, "/does/not/exist", "/does/not/exist2")
            self.assertRaises(ScriptError, port.wdiff_text, "/does/not/exist", "/does/not/exist2")
            # wdiff will still be available after running wdiff_text with invalid paths.
            self.assertTrue(port._wdiff_available)

        # If wdiff does not exist _run_wdiff should throw an OSError.
        port._path_to_wdiff = lambda: "/invalid/path/to/wdiff"
        self.assertRaises(OSError, port._run_wdiff, "foo", "bar")

        # wdiff_text should not throw an error if wdiff does not exist.
        self.assertEqual(port.wdiff_text("foo", "bar"), "")
        # However wdiff should not be available after running wdiff_text if wdiff is missing.
        self.assertFalse(port._wdiff_available)

    def test_wdiff_text(self):
        port = self.make_port()
        port.wdiff_available = lambda: True
        port._run_wdiff = lambda a, b: 'PASS'
        self.assertEqual('PASS', port.wdiff_text(None, None))

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

        # And make sure we actually get diff output.
        diff = port.diff_text('foo', 'bar', 'exp.txt', 'act.txt')
        self.assertTrue('foo' in diff)
        self.assertTrue('bar' in diff)
        self.assertTrue('exp.txt' in diff)
        self.assertTrue('act.txt' in diff)
        self.assertFalse('nosuchthing' in diff)

    def test_default_configuration_notfound(self):
        # Test that we delegate to the config object properly.
        port = self.make_port(config=config_mock.MockConfig(default_configuration='default'))
        self.assertEqual(port.default_configuration(), 'default')

    def test_layout_tests_skipping(self):
        filesystem = MockFileSystem({
            '/mock-checkout/LayoutTests/media/video-zoom.html': '',
            '/mock-checkout/LayoutTests/foo/bar.html': '',
        })
        port = self.make_port(filesystem=filesystem)
        port.skipped_layout_tests = lambda: ['foo/bar.html', 'media']
        self.assertTrue(port.skips_layout_test('foo/bar.html'))
        self.assertTrue(port.skips_layout_test('media/video-zoom.html'))
        self.assertFalse(port.skips_layout_test('foo/foo.html'))

    def test_setup_test_run(self):
        port = self.make_port()
        # This routine is a no-op. We just test it for coverage.
        port.setup_test_run()

    def test_test_dirs(self):
        filesystem = MockFileSystem({
            '/mock-checkout/LayoutTests/canvas/test': '',
            '/mock-checkout/LayoutTests/css2.1/test': '',
        })
        port = self.make_port(filesystem=filesystem)
        dirs = port.test_dirs()
        self.assertTrue('canvas' in dirs)
        self.assertTrue('css2.1' in dirs)

    def test_test_to_uri(self):
        port = self.make_port()
        layout_test_dir = port.layout_tests_dir()
        test = 'foo/bar.html'
        path = port._filesystem.join(layout_test_dir, test)
        if sys.platform == 'win32':
            path = path.replace("\\", "/")

        self.assertEqual(port.test_to_uri(test), abspath_to_uri(path))

    def test_get_option__set(self):
        options, args = optparse.OptionParser().parse_args([])
        options.foo = 'bar'
        port = self.make_port(options=options)
        self.assertEqual(port.get_option('foo'), 'bar')

    def test_get_option__unset(self):
        port = self.make_port()
        self.assertEqual(port.get_option('foo'), None)

    def test_get_option__default(self):
        port = self.make_port()
        self.assertEqual(port.get_option('foo', 'bar'), 'bar')

    def test_name__unset(self):
        port = self.make_port()
        self.assertEqual(port.name(), None)

    def test_name__set(self):
        port = self.make_port(port_name='foo')
        self.assertEqual(port.name(), 'foo')

    def test_additional_platform_directory(self):
        filesystem = MockFileSystem()
        port = self.make_port(port_name='foo', filesystem=filesystem)
        port.baseline_search_path = lambda: ['LayoutTests/platform/foo']
        layout_test_dir = port.layout_tests_dir()
        test_file = 'fast/test.html'

        # No additional platform directory
        self.assertEqual(
            port.expected_baselines(test_file, '.txt'),
            [(None, 'fast/test-expected.txt')])
        self.assertEqual(port.baseline_path(), 'LayoutTests/platform/foo')

        # Simple additional platform directory
        port._options.additional_platform_directory = ['/tmp/local-baselines']
        filesystem.files = {
            '/tmp/local-baselines/fast/test-expected.txt': 'foo',
        }
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

    def test_uses_test_expectations_file(self):
        filesystem = MockFileSystem()
        port = self.make_port(port_name='foo', filesystem=filesystem)
        port.path_to_test_expectations_file = lambda: '/mock-results/test_expectations.txt'
        self.assertFalse(port.uses_test_expectations_file())
        port._filesystem = MockFileSystem({'/mock-results/test_expectations.txt': ''})
        self.assertTrue(port.uses_test_expectations_file())

    def test_find_no_paths_specified(self):
        port = TestPort()
        layout_tests_dir = port.layout_tests_dir()
        tests = port.tests([])
        self.assertNotEqual(len(tests), 0)

    def test_find_one_test(self):
        port = TestPort()
        tests = port.tests(['failures/expected/image.html'])
        self.assertEqual(len(tests), 1)

    def test_find_glob(self):
        port = TestPort()
        tests = port.tests(['failures/expected/im*'])
        self.assertEqual(len(tests), 2)

    def test_find_with_skipped_directories(self):
        port = TestPort()
        tests = port.tests('userscripts')
        self.assertTrue('userscripts/resources/iframe.html' not in tests)

    def test_find_with_skipped_directories_2(self):
        port = TestPort()
        tests = port.tests(['userscripts/resources'])
        self.assertEqual(tests, set([]))

    def test_is_test_file(self):
        filesystem = MockFileSystem()
        self.assertTrue(Port._is_test_file(filesystem, '', 'foo.html'))
        self.assertTrue(Port._is_test_file(filesystem, '', 'foo.shtml'))
        self.assertTrue(Port._is_test_file(filesystem, '', 'test-ref-test.html'))
        self.assertFalse(Port._is_test_file(filesystem, '', 'foo.png'))
        self.assertFalse(Port._is_test_file(filesystem, '', 'foo-expected.html'))
        self.assertFalse(Port._is_test_file(filesystem, '', 'foo-expected-mismatch.html'))
        self.assertFalse(Port._is_test_file(filesystem, '', 'foo-ref.html'))
        self.assertFalse(Port._is_test_file(filesystem, '', 'foo-notref.html'))
        self.assertFalse(Port._is_test_file(filesystem, '', 'foo-notref.xht'))
        self.assertFalse(Port._is_test_file(filesystem, '', 'foo-ref.xhtml'))
        self.assertFalse(Port._is_test_file(filesystem, '', 'ref-foo.html'))
        self.assertFalse(Port._is_test_file(filesystem, '', 'notref-foo.xhr'))

    def test_parse_reftest_list(self):
        port = TestPort()
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


class VirtualTest(unittest.TestCase):
    """Tests that various methods expected to be virtual are."""
    def assertVirtual(self, method, *args, **kwargs):
        self.assertRaises(NotImplementedError, method, *args, **kwargs)

    def test_virtual_methods(self):
        port = Port(MockHost())
        self.assertVirtual(port.baseline_path)
        self.assertVirtual(port.baseline_search_path)
        self.assertVirtual(port.check_build, None)
        self.assertVirtual(port.check_image_diff)
        self.assertVirtual(port.create_driver, 0)
        self.assertVirtual(port.diff_image, None, None)
        self.assertVirtual(port.path_to_test_expectations_file)
        self.assertVirtual(port.default_results_directory)
        self.assertVirtual(port.test_expectations)
        self.assertVirtual(port._path_to_apache)
        self.assertVirtual(port._path_to_apache_config_file)
        self.assertVirtual(port._path_to_driver)
        self.assertVirtual(port._path_to_helper)
        self.assertVirtual(port._path_to_image_diff)
        self.assertVirtual(port._path_to_lighttpd)
        self.assertVirtual(port._path_to_lighttpd_modules)
        self.assertVirtual(port._path_to_lighttpd_php)
        self.assertVirtual(port._path_to_wdiff)

    def test_virtual_driver_methods(self):
        driver = Driver(Port(MockHost()), None, pixel_tests=False)
        self.assertVirtual(driver.run_test, None)
        self.assertVirtual(driver.stop)
        self.assertVirtual(driver.cmd_line)


# FIXME: This should be moved to driver_unittest.py or just deleted.
class DriverTest(unittest.TestCase):

    def _assert_wrapper(self, wrapper_string, expected_wrapper):
        wrapper = Driver(Port(MockHost()), None, pixel_tests=False)._command_wrapper(wrapper_string)
        self.assertEqual(wrapper, expected_wrapper)

    def test_command_wrapper(self):
        self._assert_wrapper(None, [])
        self._assert_wrapper("valgrind", ["valgrind"])

        # Validate that shlex works as expected.
        command_with_spaces = "valgrind --smc-check=\"check with spaces!\" --foo"
        expected_parse = ["valgrind", "--smc-check=check with spaces!", "--foo"]
        self._assert_wrapper(command_with_spaces, expected_parse)


if __name__ == '__main__':
    unittest.main()

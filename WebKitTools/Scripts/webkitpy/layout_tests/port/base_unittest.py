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

import base
import os
import StringIO
import sys
import tempfile
import unittest

from webkitpy.common.system.executive import Executive, ScriptError
from webkitpy.thirdparty.mock import Mock


# FIXME: This makes StringIO objects work with "with". Remove
# when we upgrade to 2.6.
class NewStringIO(StringIO.StringIO):
    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        pass


class MockExecutive():
    def __init__(self, exception):
        self._exception = exception

    def run_command(self, *args, **kwargs):
        raise self._exception


class UnitTestPort(base.Port):
    """Subclass of base.Port used for unit testing."""
    def __init__(self, configuration_contents=None, configuration_exception=IOError, executive_exception=None):
        base.Port.__init__(self)
        self._configuration_contents = configuration_contents
        self._configuration_exception = configuration_exception
        if executive_exception:
            self._executive = MockExecutive(executive_exception)

    def _open_configuration_file(self):
        if self._configuration_contents:
            return NewStringIO(self._configuration_contents)
        raise self._configuration_exception


class PortTest(unittest.TestCase):
    def test_format_wdiff_output_as_html(self):
        output = "OUTPUT %s %s %s" % (base.Port._WDIFF_DEL, base.Port._WDIFF_ADD, base.Port._WDIFF_END)
        html = base.Port()._format_wdiff_output_as_html(output)
        expected_html = "<head><style>.del { background: #faa; } .add { background: #afa; }</style></head><pre>OUTPUT <span class=del> <span class=add> </span></pre>"
        self.assertEqual(html, expected_html)

    def test_wdiff_command(self):
        port = base.Port()
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
        port = UnitTestPort(executive_exception=OSError)
        self.assertEqual(port.pretty_patch_text("patch.txt"),
                         port._pretty_patch_error_html)

        # This tests repeated calls to make sure we cache the result.
        self.assertEqual(port.pretty_patch_text("patch.txt"),
                         port._pretty_patch_error_html)

    def test_pretty_patch_script_error(self):
        # FIXME: This is some ugly white-box test hacking ...
        base._pretty_patch_available = True
        port = UnitTestPort(executive_exception=ScriptError)
        self.assertEqual(port.pretty_patch_text("patch.txt"),
                         port._pretty_patch_error_html)

        # This tests repeated calls to make sure we cache the result.
        self.assertEqual(port.pretty_patch_text("patch.txt"),
                         port._pretty_patch_error_html)

    def test_run_wdiff(self):
        executive = Executive()
        # This may fail on some systems.  We could ask the port
        # object for the wdiff path, but since we don't know what
        # port object to use, this is sufficient for now.
        try:
            wdiff_path = executive.run_command(["which", "wdiff"]).rstrip()
        except Exception, e:
            wdiff_path = None

        port = base.Port()
        port._path_to_wdiff = lambda: wdiff_path

        if wdiff_path:
            # "with tempfile.NamedTemporaryFile() as actual" does not seem to work in Python 2.5
            actual = self._file_with_contents(u"foo")
            expected = self._file_with_contents(u"bar")
            wdiff = port._run_wdiff(actual.name, expected.name)
            expected_wdiff = "<head><style>.del { background: #faa; } .add { background: #afa; }</style></head><pre><span class=del>foo</span><span class=add>bar</span></pre>"
            self.assertEqual(wdiff, expected_wdiff)
            # Running the full wdiff_text method should give the same result.
            base._wdiff_available = True  # In case it's somehow already disabled.
            wdiff = port.wdiff_text(actual.name, expected.name)
            self.assertEqual(wdiff, expected_wdiff)
            # wdiff should still be available after running wdiff_text with a valid diff.
            self.assertTrue(base._wdiff_available)
            actual.close()
            expected.close()

            # Bogus paths should raise a script error.
            self.assertRaises(ScriptError, port._run_wdiff, "/does/not/exist", "/does/not/exist2")
            self.assertRaises(ScriptError, port.wdiff_text, "/does/not/exist", "/does/not/exist2")
            # wdiff will still be available after running wdiff_text with invalid paths.
            self.assertTrue(base._wdiff_available)
            base._wdiff_available = True

        # If wdiff does not exist _run_wdiff should throw an OSError.
        port._path_to_wdiff = lambda: "/invalid/path/to/wdiff"
        self.assertRaises(OSError, port._run_wdiff, "foo", "bar")

        # wdiff_text should not throw an error if wdiff does not exist.
        self.assertEqual(port.wdiff_text("foo", "bar"), "")
        # However wdiff should not be available after running wdiff_text if wdiff is missing.
        self.assertFalse(base._wdiff_available)
        base._wdiff_available = True

    def test_diff_text(self):
        port = base.Port()
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
        # Regular IOError thrown while trying to get the configuration.
        port = UnitTestPort()
        self.assertEqual(port.default_configuration(), "Release")

        # More exotic OSError thrown.
        port = UnitTestPort(configuration_exception=OSError)
        self.assertEqual(port.default_configuration(), "Release")

    def test_layout_tests_skipping(self):
        port = base.Port()
        port.skipped_layout_tests = lambda: ['foo/bar.html', 'media']
        self.assertTrue(port.skips_layout_test('foo/bar.html'))
        self.assertTrue(port.skips_layout_test('media/video-zoom.html'))
        self.assertFalse(port.skips_layout_test('foo/foo.html'))

    def test_default_configuration_found(self):
        port = UnitTestPort(configuration_contents="Debug")
        self.assertEqual(port.default_configuration(), "Debug")

    def test_default_configuration_unknown(self):
        port = UnitTestPort(configuration_contents="weird_value")
        self.assertEqual(port.default_configuration(), "weird_value")

    def test_setup_test_run(self):
        port = base.Port()
        # This routine is a no-op. We just test it for coverage.
        port.setup_test_run()

    def test_test_dirs(self):
        port = base.Port()
        dirs = port.test_dirs()
        self.assertTrue('canvas' in dirs)
        self.assertTrue('css2.1' in dirs)

class VirtualTest(unittest.TestCase):
    """Tests that various methods expected to be virtual are."""
    def assertVirtual(self, method, *args, **kwargs):
        self.assertRaises(NotImplementedError, method, *args, **kwargs)

    def test_virtual_methods(self):
        port = base.Port()
        self.assertVirtual(port.baseline_path)
        self.assertVirtual(port.baseline_search_path)
        self.assertVirtual(port.check_build, None)
        self.assertVirtual(port.check_image_diff)
        self.assertVirtual(port.create_driver, None, None)
        self.assertVirtual(port.diff_image, None, None)
        self.assertVirtual(port.path_to_test_expectations_file)
        self.assertVirtual(port.test_platform_name)
        self.assertVirtual(port.results_directory)
        self.assertVirtual(port.test_expectations)
        self.assertVirtual(port.test_base_platform_names)
        self.assertVirtual(port.test_platform_name)
        self.assertVirtual(port.test_platforms)
        self.assertVirtual(port.test_platform_name_to_name, None)
        self.assertVirtual(port.version)
        self.assertVirtual(port._path_to_apache)
        self.assertVirtual(port._path_to_apache_config_file)
        self.assertVirtual(port._path_to_driver)
        self.assertVirtual(port._path_to_helper)
        self.assertVirtual(port._path_to_image_diff)
        self.assertVirtual(port._path_to_lighttpd)
        self.assertVirtual(port._path_to_lighttpd_modules)
        self.assertVirtual(port._path_to_lighttpd_php)
        self.assertVirtual(port._path_to_wdiff)
        self.assertVirtual(port._shut_down_http_server, None)

    def test_virtual_driver_method(self):
        self.assertRaises(NotImplementedError, base.Driver, base.Port(),
                          "", None, None)

    def test_virtual_driver_methods(self):
        class VirtualDriver(base.Driver):
            def __init__(self):
                pass

        driver = VirtualDriver()
        self.assertVirtual(driver.run_test, None, None, None)
        self.assertVirtual(driver.poll)
        self.assertVirtual(driver.stop)


class DriverTest(unittest.TestCase):

    def _assert_wrapper(self, wrapper_string, expected_wrapper):
        wrapper = base.Driver._command_wrapper(wrapper_string)
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

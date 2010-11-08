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

import codecs
import os
import StringIO
import unittest

from webkitpy.common.system import outputcapture

import config


# FIXME: This makes StringIO objects work with "with". Remove
# when we upgrade to 2.6.
class NewStringIO(StringIO.StringIO):
    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        pass


class MockExecutive(object):
    def __init__(self, output='', exit_code=0):
        self._output = output
        self._exit_code = exit_code

    def run_command(self, arg_list, return_exit_code=False,
                    decode_output=False):
        if return_exit_code:
            return self._exit_code
        return self._output


class ConfigTest(unittest.TestCase):
    def tearDown(self):
        config.clear_cached_configuration()

    def assertConfiguration(self, contents, expected):
        # This tests that a configuration file containing
        # _contents_ endsd up being interpreted as _expected_.
        #
        # FIXME: rewrite this when we have a filesystem abstraction we
        # can mock out more easily.
        config.clear_cached_configuration()
        orig_open = codecs.open

        def wrap_open(contents):
            def open_wrapper(path, mode, encoding):
                return NewStringIO(contents)
            return open_wrapper

        try:
            orig_exists = os.path.exists
            os.path.exists = lambda p: True
            codecs.open = wrap_open(contents)

            e = MockExecutive(output='foo')
            c = config.Config(e)
            self.assertEqual(c.default_configuration(), expected)
        finally:
            os.path.exists = orig_exists
            codecs.open = orig_open

    def test_build_directory_toplevel(self):
        e = MockExecutive(output="toplevel")
        c = config.Config(e)
        self.assertEqual(c.build_directory(None), 'toplevel')

        # Test again to check caching
        self.assertEqual(c.build_directory(None), 'toplevel')

    def test_build_directory__release(self):
        e = MockExecutive(output="release")
        c = config.Config(e)
        self.assertEqual(c.build_directory('Release'), 'release')

    def test_build_directory__debug(self):
        e = MockExecutive(output="debug")
        c = config.Config(e)
        self.assertEqual(c.build_directory('Debug'), 'debug')

    def test_build_directory__unknown(self):
        e = MockExecutive(output="unknown")
        c = config.Config(e)
        self.assertRaises(KeyError, c.build_directory, 'Unknown')

    def test_build_dumprendertree__success(self):
        e = MockExecutive(exit_code=0)
        c = config.Config(e)
        self.assertTrue(c.build_dumprendertree("Debug"))
        self.assertTrue(c.build_dumprendertree("Release"))
        self.assertRaises(KeyError, c.build_dumprendertree, "Unknown")

    def test_build_dumprendertree__failure(self):
        e = MockExecutive(exit_code=-1)
        c = config.Config(e)

        oc = outputcapture.OutputCapture()
        oc.capture_output()
        self.assertFalse(c.build_dumprendertree('Debug'))
        (out, err) = oc.restore_output()

        oc.capture_output()
        self.assertFalse(c.build_dumprendertree('Release'))
        oc.restore_output()

    def test_default_configuration__release(self):
        self.assertConfiguration('Release', 'Release')

    def test_default_configuration__debug(self):
        self.assertConfiguration('Debug', 'Debug')

    def test_default_configuration__deployment(self):
        self.assertConfiguration('Deployment', 'Release')

    def test_default_configuration__development(self):
        self.assertConfiguration('Development', 'Debug')

    def test_default_configuration__notfound(self):
        # This tests what happens if the default configuration file
        # doesn't exist.
        config.clear_cached_configuration()
        try:
            orig_exists = os.path.exists
            os.path.exists = lambda p: False
            e = MockExecutive(output="foo")
            c = config.Config(e)
            self.assertEqual(c.default_configuration(), "Release")
        finally:
            os.path.exists = orig_exists

    def test_default_configuration__unknown(self):
        # Ignore the warning about an unknown configuration value.
        oc = outputcapture.OutputCapture()
        oc.capture_output()
        self.assertConfiguration('Unknown', 'Unknown')
        oc.restore_output()

    def test_path_from_webkit_base(self, *comps):
        c = config.Config(None)
        self.assertTrue(c.path_from_webkit_base('foo'))

    def test_webkit_base_dir(self):
        c = config.Config(None)
        self.assertTrue(c.webkit_base_dir())


if __name__ == '__main__':
    unittest.main()

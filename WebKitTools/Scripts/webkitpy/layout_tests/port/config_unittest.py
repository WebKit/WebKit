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

from webkitpy.common.system import executive
from webkitpy.common.system import executive_mock
from webkitpy.common.system import filesystem
from webkitpy.common.system import filesystem_mock
from webkitpy.common.system import outputcapture

import config

class ConfigTest(unittest.TestCase):
    def tearDown(self):
        config.clear_cached_configuration()

    def make_config(self, output='', files={}, exit_code=0):
        e = executive_mock.MockExecutive2(output=output, exit_code=exit_code)
        fs = filesystem_mock.MockFileSystem(files)
        return config.Config(e, fs)

    def assert_configuration(self, contents, expected):
        # This tests that a configuration file containing
        # _contents_ endsd up being interpreted as _expected_.
        c = self.make_config('foo', {'foo/Configuration': contents})
        self.assertEqual(c.default_configuration(), expected)

    def test_build_directory_toplevel(self):
        c = self.make_config('toplevel')
        self.assertEqual(c.build_directory(None), 'toplevel')

        # Test again to check caching
        self.assertEqual(c.build_directory(None), 'toplevel')

    def test_build_directory__release(self):
        c = self.make_config('release')
        self.assertEqual(c.build_directory('Release'), 'release')

    def test_build_directory__debug(self):
        c = self.make_config('debug')
        self.assertEqual(c.build_directory('Debug'), 'debug')

    def test_build_directory__unknown(self):
        c = self.make_config("unknown")
        self.assertRaises(KeyError, c.build_directory, 'Unknown')

    def test_build_dumprendertree__success(self):
        c = self.make_config(exit_code=0)
        self.assertTrue(c.build_dumprendertree("Debug"))
        self.assertTrue(c.build_dumprendertree("Release"))
        self.assertRaises(KeyError, c.build_dumprendertree, "Unknown")

    def test_build_dumprendertree__failure(self):
        c = self.make_config(exit_code=-1)

        # FIXME: Build failures should log errors. However, the message we
        # get depends on how we're being called; as a standalone test,
        # we'll get the "no handlers found" message. As part of
        # test-webkitpy, we get the actual message. Really, we need
        # outputcapture to install its own handler.
        oc = outputcapture.OutputCapture()
        oc.capture_output()
        self.assertFalse(c.build_dumprendertree('Debug'))
        oc.restore_output()

        oc.capture_output()
        self.assertFalse(c.build_dumprendertree('Release'))
        oc.restore_output()

    def test_default_configuration__release(self):
        self.assert_configuration('Release', 'Release')

    def test_default_configuration__debug(self):
        self.assert_configuration('Debug', 'Debug')

    def test_default_configuration__deployment(self):
        self.assert_configuration('Deployment', 'Release')

    def test_default_configuration__development(self):
        self.assert_configuration('Development', 'Debug')

    def test_default_configuration__notfound(self):
        # This tests what happens if the default configuration file
        # doesn't exist.
        c = self.make_config(output='foo', files={'foo/Configuration': None})
        self.assertEqual(c.default_configuration(), "Release")

    def test_default_configuration__unknown(self):
        # Ignore the warning about an unknown configuration value.
        oc = outputcapture.OutputCapture()
        oc.capture_output()
        self.assert_configuration('Unknown', 'Unknown')
        oc.restore_output()

    def test_path_from_webkit_base(self):
        # FIXME: We use a real filesystem here. Should this move to a
        # mocked one?
        c = config.Config(executive.Executive(), filesystem.FileSystem())
        self.assertTrue(c.path_from_webkit_base('foo'))

    def test_webkit_base_dir(self):
        # FIXME: We use a real filesystem here. Should this move to a
        # mocked one?
        c = config.Config(executive.Executive(), filesystem.FileSystem())
        self.assertTrue(c.webkit_base_dir())


if __name__ == '__main__':
    unittest.main()

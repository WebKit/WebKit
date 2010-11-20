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

import os
import sys
import unittest

from webkitpy.common.system import executive
from webkitpy.common.system import executive_mock
from webkitpy.common.system import filesystem
from webkitpy.common.system import filesystem_mock
from webkitpy.common.system import outputcapture

import config


def mock_run_command(arg_list):
    # Set this to True to test actual output (where possible).
    integration_test = False
    if integration_test:
        return executive.Executive().run_command(arg_list)

    if 'webkit-build-directory' in arg_list[1]:
        return mock_webkit_build_directory(arg_list[2:])
    return 'Error'


def mock_webkit_build_directory(arg_list):
    if arg_list == ['--top-level']:
        return '/WebKitBuild'
    elif arg_list == ['--configuration', '--debug']:
        return '/WebKitBuild/Debug'
    elif arg_list == ['--configuration', '--release']:
        return '/WebKitBuild/Release'
    return 'Error'


class ConfigTest(unittest.TestCase):
    def tearDown(self):
        config.clear_cached_configuration()

    def make_config(self, output='', files={}, exit_code=0, exception=None,
                    run_command_fn=None):
        e = executive_mock.MockExecutive2(output=output, exit_code=exit_code,
                                          exception=exception,
                                          run_command_fn=run_command_fn)
        fs = filesystem_mock.MockFileSystem(files)
        return config.Config(e, fs)

    def assert_configuration(self, contents, expected):
        # This tests that a configuration file containing
        # _contents_ ends up being interpreted as _expected_.
        c = self.make_config('foo', {'foo/Configuration': contents})
        self.assertEqual(c.default_configuration(), expected)

    def test_build_directory(self):
        # --top-level
        c = self.make_config(run_command_fn=mock_run_command)
        self.assertTrue(c.build_directory(None).endswith('WebKitBuild'))

        # Test again to check caching
        self.assertTrue(c.build_directory(None).endswith('WebKitBuild'))

        # Test other values
        self.assertTrue(c.build_directory('Release').endswith('/Release'))
        self.assertTrue(c.build_directory('Debug').endswith('/Debug'))
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

    def test_default_configuration__standalone(self):
        # FIXME: This test runs a standalone python script to test
        # reading the default configuration to work around any possible
        # caching / reset bugs. See https://bugs.webkit.org/show_bug?id=49360
        # for the motivation. We can remove this test when we remove the
        # global configuration cache in config.py.
        e = executive.Executive()
        fs = filesystem.FileSystem()
        c = config.Config(e, fs)
        script = c.path_from_webkit_base('WebKitTools', 'Scripts',
            'webkitpy', 'layout_tests', 'port', 'config_standalone.py')

        # Note: don't use 'Release' here, since that's the normal default.
        expected = 'Debug'

        args = [sys.executable, script, '--mock', expected]
        actual = e.run_command(args).rstrip()
        self.assertEqual(actual, expected)

    def test_default_configuration__no_perl(self):
        # We need perl to run webkit-build-directory to find out where the
        # default configuration file is. See what happens if perl isn't
        # installed. (We should get the default value, 'Release').
        c = self.make_config(exception=OSError)
        actual = c.default_configuration()
        self.assertEqual(actual, 'Release')

    def test_default_configuration__scripterror(self):
        # We run webkit-build-directory to find out where the default
        # configuration file is. See what happens if that script fails.
        # (We should get the default value, 'Release').
        c = self.make_config(exception=executive.ScriptError())
        actual = c.default_configuration()
        self.assertEqual(actual, 'Release')

    def test_path_from_webkit_base(self):
        # FIXME: We use a real filesystem here. Should this move to a
        # mocked one?
        c = config.Config(executive.Executive(), filesystem.FileSystem())
        self.assertTrue(c.path_from_webkit_base('foo'))

    def test_webkit_base_dir(self):
        # FIXME: We use a real filesystem here. Should this move to a
        # mocked one?
        c = config.Config(executive.Executive(), filesystem.FileSystem())
        base_dir = c.webkit_base_dir()
        self.assertTrue(base_dir)

        orig_cwd = os.getcwd()
        os.chdir(os.environ['HOME'])
        c = config.Config(executive.Executive(), filesystem.FileSystem())
        try:
            base_dir_2 = c.webkit_base_dir()
            self.assertEqual(base_dir, base_dir_2)
        finally:
            os.chdir(orig_cwd)


if __name__ == '__main__':
    unittest.main()

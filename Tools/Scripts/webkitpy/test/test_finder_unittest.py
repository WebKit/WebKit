# Copyright (C) 2012 Google, Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging
import unittest

from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.test.test_finder import TestFinder


class TestFinderTest(unittest.TestCase):
    def setUp(self):
        files = {
          '/foo/bar/baz.py': '',
          '/foo/bar/baz_unittest.py': '',
          '/foo2/bar2/baz2.py': '',
          '/foo2/bar2/baz2.pyc': '',
          '/foo2/bar2/baz2_integrationtest.py': '',
          '/foo2/bar2/missing.pyc': '',
          '/tmp/another_unittest.py': '',
        }
        self.fs = MockFileSystem(files)
        self.finder = TestFinder(self.fs)
        self.finder.add_tree('/foo', 'bar')
        self.finder.add_tree('/foo2')

        # Here we have to jump through a hoop to make sure test-webkitpy doesn't log
        # any messages from these tests :(.
        self.root_logger = logging.getLogger()
        self.log_handler = None
        for h in self.root_logger.handlers:
            if getattr(h, 'name', None) == 'webkitpy.test.main':
                self.log_handler = h
                break
        if self.log_handler:
            self.log_level = self.log_handler.level
            self.log_handler.level = logging.CRITICAL

    def tearDown(self):
        if self.log_handler:
            self.log_handler.setLevel(self.log_level)

    def test_additional_system_paths(self):
        self.assertEquals(self.finder.additional_paths(['/usr']),
                          ['/foo', '/foo2'])

    def test_is_module(self):
        self.assertTrue(self.finder.is_module('bar.baz'))
        self.assertTrue(self.finder.is_module('bar2.baz2'))
        self.assertTrue(self.finder.is_module('bar2.baz2_integrationtest'))

        # Missing the proper namespace.
        self.assertFalse(self.finder.is_module('baz'))

    def test_to_module(self):
        self.assertEquals(self.finder.to_module('/foo/test.py'), 'test')
        self.assertEquals(self.finder.to_module('/foo/bar/test.py'), 'bar.test')
        self.assertEquals(self.finder.to_module('/foo/bar/pytest.py'), 'bar.pytest')

    def test_clean(self):
        self.assertTrue(self.fs.exists('/foo2/bar2/missing.pyc'))
        self.finder.clean_trees()
        self.assertFalse(self.fs.exists('/foo2/bar2/missing.pyc'))

    def check_names(self, names, expected_names, skip_integrationtests=False, find_all=False):
        self.assertEquals(self.finder.find_names(names, skip_integrationtests, find_all),
                          expected_names)

    def test_default_names(self):
        self.check_names([], ['bar.baz_unittest', 'bar2.baz2_integrationtest'])
        self.check_names([], ['bar.baz_unittest'], skip_integrationtests=True, find_all=True)
        self.check_names([], ['bar.baz_unittest'], skip_integrationtests=True, find_all=False)

        # Should return the names given it, even if they don't exist.
        self.check_names(['foobar'], ['foobar'], skip_integrationtests=True, find_all=False)

    def test_paths(self):
        self.fs.chdir('/foo/bar')
        self.check_names(['baz_unittest.py'], ['bar.baz_unittest'])
        self.check_names(['./baz_unittest.py'], ['bar.baz_unittest'])
        self.check_names(['/foo/bar/baz_unittest.py'], ['bar.baz_unittest'])
        self.check_names(['.'], ['bar.baz_unittest'])
        self.check_names(['../../foo2/bar2'], ['bar2.baz2_integrationtest'])

        self.fs.chdir('/')
        self.check_names(['bar'], ['bar.baz_unittest'])
        self.check_names(['/foo/bar/'], ['bar.baz_unittest'])

        # This works 'by accident' since it maps onto a package.
        self.check_names(['bar/'], ['bar.baz_unittest'])

        # This should log an error, since it's outside the trees.
        oc = OutputCapture()
        oc.set_log_level(logging.ERROR)
        oc.capture_output()
        try:
            self.check_names(['/tmp/another_unittest.py'], [])
        finally:
            _, _, logs = oc.restore_output()
            self.assertTrue('another_unittest.py' in logs)

        # Paths that don't exist are errors.
        oc.capture_output()
        try:
            self.check_names(['/foo/bar/notexist_unittest.py'], [])
        finally:
            _, _, logs = oc.restore_output()
            self.assertTrue('notexist_unittest.py' in logs)

        # Names that don't exist are caught later, at load time.
        self.check_names(['bar.notexist_unittest'], ['bar.notexist_unittest'])

if __name__ == '__main__':
    unittest.main()

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
        }
        self.fs = MockFileSystem(files)
        self.finder = TestFinder(self.fs)
        self.finder.add_tree('/foo', 'bar')
        self.finder.add_tree('/foo2')
        self.root_logger = logging.getLogger()
        self.log_level = self.root_logger.level
        self.root_logger.setLevel(logging.WARNING)

    def tearDown(self):
        self.root_logger.setLevel(self.log_level)

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

    def test_find_names(self):
        self.assertEquals(self.finder.find_names([], skip_integrationtests=False, find_all=True),
            ['bar.baz_unittest', 'bar2.baz2_integrationtest'])

        self.assertEquals(self.finder.find_names([], skip_integrationtests=True, find_all=True),
            ['bar.baz_unittest'])
        self.assertEquals(self.finder.find_names([], skip_integrationtests=True, find_all=False),
            ['bar.baz_unittest'])

        # Should return the names given it, even if they don't exist.
        self.assertEquals(self.finder.find_names(['foobar'], skip_integrationtests=True, find_all=True),
            ['foobar'])


if __name__ == '__main__':
    unittest.main()

# Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above
#    copyright notice, this list of conditions and the following
#    disclaimer.
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

import optparse
import shutil
import tempfile
import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.executive_mock import MockExecutive2, ScriptError
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.w3c.test_importer import TestImporter


FAKE_SOURCE_DIR = '/tests/csswg'
FAKE_TEST_PATH = 'css-fake-1'

FAKE_FILES = {
    '/tests/csswg/css-fake-1/empty_dir/README.txt': '',
    '/mock-checkout/LayoutTests/w3c/css-fake-1/README.txt': '',
}

class TestImporterTest(unittest.TestCase):

    def test_import_dir_with_no_tests_and_no_hg(self):
        host = MockHost()
        host.executive = MockExecutive2(exception=OSError())
        host.filesystem = MockFileSystem(files=FAKE_FILES)

        importer = TestImporter(host, FAKE_SOURCE_DIR, optparse.Values({"overwrite": False, 'destination': 'w3c', 'test_paths': [FAKE_TEST_PATH]}))

        oc = OutputCapture()
        oc.capture_output()
        try:
            importer.do_import()
        finally:
            oc.restore_output()

    def test_import_dir_with_no_tests(self):
        host = MockHost()
        host.executive = MockExecutive2(exception=ScriptError("abort: no repository found in '/Volumes/Source/src/wk/Tools/Scripts/webkitpy/w3c' (.hg not found)!"))
        host.filesystem = MockFileSystem(files=FAKE_FILES)

        importer = TestImporter(host, FAKE_SOURCE_DIR, optparse.Values({"overwrite": False, 'destination': 'w3c', 'test_paths': [FAKE_TEST_PATH]}))
        oc = OutputCapture()
        oc.capture_output()
        try:
            importer.do_import()
        finally:
            oc.restore_output()

    def test_import_dir_with_empty_init_py(self):
        FAKE_FILES = {
            '/tests/csswg/test1/__init__.py': '',
            '/tests/csswg/test2/__init__.py': 'NOTEMPTY',
        }

        host = MockHost()
        host.filesystem = MockFileSystem(files=FAKE_FILES)

        importer = TestImporter(host, FAKE_SOURCE_DIR, optparse.Values({"overwrite": False, 'destination': 'w3c', 'test_paths': ['/tests/csswg']}))
        importer.do_import()

        self.assertTrue(host.filesystem.exists("/mock-checkout/LayoutTests/w3c/test1/__init__.py"))
        self.assertTrue(host.filesystem.exists("/mock-checkout/LayoutTests/w3c/test2/__init__.py"))
        self.assertTrue(host.filesystem.getsize("/mock-checkout/LayoutTests/w3c/test1/__init__.py") > 0)

    # FIXME: Needs more tests.

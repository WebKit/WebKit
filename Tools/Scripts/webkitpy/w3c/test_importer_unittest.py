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
from webkitpy.w3c.test_downloader import TestDownloader
from webkitpy.w3c.test_importer import parse_args, TestImporter


FAKE_SOURCE_DIR = '/tests/csswg'
FAKE_TEST_PATH = 'css-fake-1'

FAKE_FILES = {
    '/tests/csswg/css-fake-1/empty_dir/README.txt': '',
    '/mock-checkout/LayoutTests/w3c/css-fake-1/README.txt': '',
}

FAKE_REPOSITORY = {
    '/mock-checkout/LayoutTests/imported/w3c/resources/TestRepositories': '''
[
    {
        "name": "csswg-tests",
        "url": "https://github.com/w3c/csswg-test.git",
        "revision": "9f45f89",
        "paths_to_skip": [],
        "paths_to_import": [],
        "import_options": ["convert_test_harness_links"]
    },
    {
        "name": "web-platform-tests",
        "url": "https://github.com/w3c/web-platform-tests.git",
        "revision": "dd553279c3",
        "paths_to_skip": [],
        "paths_to_import": [],
        "import_options": ["generate_git_submodules_description"]
    }
]
''' }

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

    def import_downloaded_tests(self, args, files):
        # files are passed as parameter as we cannot clone/fetch/checkout a repo in mock system.

        class TestDownloaderMock(TestDownloader):
            def __init__(self, repository_directory, host, options):
                TestDownloader.__init__(self, repository_directory, host, options)

            def _git_submodules_status(self, repository_directory):
                return 'adb4d391a69877d4a1eaaf51d1725c99a5b8ed84 tools/resources'

        host = MockHost()
        host.executive = MockExecutive2()
        host.filesystem = MockFileSystem(files=files)

        options, args = parse_args(args)
        importer = TestImporter(host, None, options)
        importer._test_downloader = TestDownloaderMock(importer.tests_download_path, importer.host, importer.options)
        importer.do_import()
        return host.filesystem

    def test_harnesslinks_conversion(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/csswg-tests/t/test.html': '<!doctype html><script src="/resources/testharness.js"></script><script src="/resources/testharnessreport.js"></script>',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/t/test.html': '<!doctype html><script src="/resources/testharness.js"></script><script src="/resources/testharnessreport.js"></script>',
            '/mock-checkout/Source/WebCore/css/CSSPropertyNames.in': '',
            '/mock-checkout/Source/WebCore/css/CSSValueKeywords.in': '',
        }
        FAKE_FILES.update(FAKE_REPOSITORY)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '-d', 'w3c'], FAKE_FILES)

        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/csswg-tests/t/test.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.html'))
        self.assertTrue('src="/resources/testharness.js"' in fs.read_text_file('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.html'))
        self.assertTrue('src="../' in fs.read_text_file('/mock-checkout/LayoutTests/w3c/csswg-tests/t/test.html'))

    def test_submodules_generation(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/csswg-tests/.gitmodules': '[submodule "tools/resources"]\n	path = tools/resources\n	url = https://github.com/w3c/testharness.js.git\n  ignore = dirty\n',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/.gitmodules': '[submodule "tools/resources"]\n	path = tools/resources\n	url = https://github.com/w3c/testharness.js.git\n  ignore = dirty\n',
        }
        FAKE_FILES.update(FAKE_REPOSITORY)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '-d', 'w3c'], FAKE_FILES)

        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/resources/csswg-tests-modules.json'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/resources/web-platform-tests-modules.json'))
        # FIXME: Mock-up of git cannot use submodule command, hence the json file is empty, but still it should be created
        #self.assertTrue('https://github.com/w3c/testharness.js/archive/db4d391a69877d4a1eaaf51d1725c99a5b8ed84.tar.gz' in fs.read_text_file('/mock-checkout/LayoutTests/w3c/resources/web-platform-tests-modules.json'))

    def test_tests_directory(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/streams-api/reference-implementation/web-platform-tests/test.html': '<!doctype html><script src="/resources/testharness.js"></script><script src="/resources/testharnessreport.js"></script>',
    '/mock-checkout/LayoutTests/imported/w3c/resources/TestRepositories': '''
[
    {
        "name": "streams-api",
        "url": "https://github.com/whatwg/streams.git",
        "revision": "7cc96dd",
        "tests_directory": "reference-implementation/web-platform-tests",
        "paths_to_skip": [],
        "paths_to_import": [],
        "import_options": []
     }
]
'''}

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '-d', 'w3c'], FAKE_FILES)

        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/streams-api/reference-implementation/web-platform-tests/test.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/streams-api/test.html'))


    # FIXME: Needs more tests.

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

import os
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
        "import_options": ["generate_git_submodules_description", "generate_gitignore", "generate_init_py"]
    }
]
''',
    '/mock-checkout/LayoutTests/imported/w3c/resources/import-expectations.json': '''
{
    "test1": "import",
    "test2": "skip"
}'''
}


class TestImporterTest(unittest.TestCase):

    def _parse_options(self, args):
        options, args = parse_args(args)
        return options

    def test_import_dir_with_no_tests_and_no_hg(self):
        FAKE_FILES.update(FAKE_REPOSITORY)

        host = MockHost()
        host.executive = MockExecutive2(exception=OSError())
        host.filesystem = MockFileSystem(files=FAKE_FILES)

        importer = TestImporter(host, FAKE_TEST_PATH, self._parse_options(['-n', '-d', 'w3c', '-s', FAKE_SOURCE_DIR]))

        oc = OutputCapture()
        oc.capture_output()
        try:
            importer.do_import()
        finally:
            oc.restore_output()

    def test_import_dir_with_no_tests(self):
        FAKE_FILES.update(FAKE_REPOSITORY)

        host = MockHost()
        host.executive = MockExecutive2(exception=ScriptError("abort: no repository found in '/Volumes/Source/src/wk/Tools/Scripts/webkitpy/w3c' (.hg not found)!"))
        host.filesystem = MockFileSystem(files=FAKE_FILES)

        importer = TestImporter(host, FAKE_TEST_PATH, self._parse_options(['-n', '-d', 'w3c', '-s', FAKE_SOURCE_DIR]))
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
        FAKE_FILES.update(FAKE_REPOSITORY)

        host = MockHost()
        host.filesystem = MockFileSystem(files=FAKE_FILES)

        importer = TestImporter(host, ['test1', 'test2'], self._parse_options(['-n', '-d', 'w3c', '-s', FAKE_SOURCE_DIR]))
        importer.do_import()

        self.assertTrue(host.filesystem.exists("/mock-checkout/LayoutTests/w3c/test1/__init__.py"))
        self.assertTrue(host.filesystem.exists("/mock-checkout/LayoutTests/w3c/test2/__init__.py"))
        self.assertTrue(host.filesystem.getsize("/mock-checkout/LayoutTests/w3c/test1/__init__.py") > 0)

    def import_directory(self, args, files, test_paths):
        host = MockHost()
        host.executive = MockExecutive2()
        host.filesystem = MockFileSystem(files=files)

        options, args = parse_args(args)
        importer = TestImporter(host, test_paths, options)
        importer.do_import()
        return host.filesystem

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
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/css/t/test.html': '<!doctype html><script src="/resources/testharness.js"></script><script src="/resources/testharnessreport.js"></script>',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/t/test.html': '<!doctype html><script src="/resources/testharness.js"></script><script src="/resources/testharnessreport.js"></script>',
            '/mock-checkout/Source/WebCore/css/CSSProperties.json': '',
            '/mock-checkout/Source/WebCore/css/CSSValueKeywords.in': '',
        }
        FAKE_FILES.update(FAKE_REPOSITORY)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '-d', 'w3c'], FAKE_FILES)

        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/t/test.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.html'))
        self.assertTrue('src="/resources/testharness.js"' in fs.read_text_file('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.html'))
        self.assertTrue('src="/resources/testharness.js"' in fs.read_text_file('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/t/test.html'))

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

    def test_skip_test_import(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/streams-api/reference-implementation/web-platform-tests/test.html': '<!doctype html><script src="/resources/testharness.js"></script><script src="/resources/testharnessreport.js"></script>',
            '/mock-checkout/LayoutTests/imported/w3c/resources/TestRepositories': '''
[
    {
        "name": "web-platform-tests",
        "url": "https://github.com/myrepo",
        "revision": "7cc96dd",
        "paths_to_skip": [],
        "paths_to_import": [],
        "import_options": []
     }
]''',
            '/mock-checkout/LayoutTests/imported/w3c/resources/import-expectations.json': '''
{
"web-platform-tests/dir-to-skip": "skip",
"web-platform-tests/dir-to-skip/dir-to-import": "import",
"web-platform-tests/dir-to-skip/file-to-import.html": "import"
}''',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/dir-to-skip/test-to-skip.html': 'to be skipped',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/dir-to-skip/dir-to-import/test-to-import.html': 'to be imported',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/dir-to-skip/dir-to-not-import/test-to-not-import.html': 'to be skipped',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/dir-to-skip/file-to-import.html': 'to be imported',
        }

        fs = self.import_downloaded_tests(['--no-fetch', '-d', 'w3c'], FAKE_FILES)

        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/dir-to-skip/file-to-import.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/dir-to-skip/dir-to-import/test-to-import.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/dir-to-skip/dir-to-not-import/test-to-not-import.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/dir-to-skip/test-to-skip.html'))

    def test_checkout_directory(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild2/w3c-tests/web-platform-tests/existing-test.html': '',
            '/mock-checkout/WebKitBuild2/w3c-tests/csswg-tests/test.html': '1',
        }

        FAKE_FILES.update(FAKE_REPOSITORY)

        os.environ['WEBKIT_OUTPUTDIR'] = '/mock-checkout/WebKitBuild2'
        try:
            fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '-d', 'w3c'], FAKE_FILES)
        finally:
            del os.environ['WEBKIT_OUTPUTDIR']

        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/existing-test.html'))

    def test_clean_directory_option(self):
        FAKE_FILES = {
            '/mock-checkout/LayoutTests/w3c/web-platform-tests/.gitattributes': '-1',
            '/mock-checkout/LayoutTests/w3c/web-platform-tests/.gitignore': '-1',
            '/mock-checkout/LayoutTests/w3c/web-platform-tests/.svn/wc.db': '0',
            '/mock-checkout/LayoutTests/w3c/web-platform-tests/old-test.html': '1',
            '/mock-checkout/LayoutTests/w3c/web-platform-tests/old-test-expected.txt': '2',
            '/mock-checkout/LayoutTests/w3c/web-platform-tests/existing-test.html': '3',
            '/mock-checkout/LayoutTests/w3c/web-platform-tests/existing-test-expected.txt': '4',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/existing-test.html': '5',
            '/mock-checkout/WebKitBuild/w3c-tests/csswg-tests/test.html': '1',
        }

        FAKE_FILES.update(FAKE_REPOSITORY)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '-d', 'w3c', '--clean-dest-dir'], FAKE_FILES)

        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/old-test.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/old-test-expected.txt'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/existing-test.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/existing-test-expected.txt'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/.gitattributes'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/.gitignore'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/.svn'))

    def test_git_ignore_generation(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/csswg-tests/.gitmodules': '[submodule "tools/resources"]\n	path = tools/resources\n	url = https://github.com/w3c/testharness.js.git\n  ignore = dirty\n',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/.gitmodules': '[submodule "tools/resources"]\n	path = tools/resources\n	url = https://github.com/w3c/testharness.js.git\n  ignore = dirty\n',
        }

        FAKE_FILES.update(FAKE_REPOSITORY)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '-d', 'w3c'], FAKE_FILES)

        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/csswg-tests/.gitignore'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/.gitignore'))
        # We should activate these lines but this is not working in mock systems.
        #self.assertTrue('/tools/.resources.url' in fs.read_text_file('/mock-checkout/LayoutTests/w3c/web-platform-tests/.gitignore'))
        #self.assertTrue('/tools/resources/' in fs.read_text_file('/mock-checkout/LayoutTests/w3c/web-platform-tests/.gitignore'))

    def test_initpy_generation(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/csswg-tests/.gitmodules': '[submodule "tools/resources"]\n	path = tools/resources\n	url = https://github.com/w3c/testharness.js.git\n  ignore = dirty\n',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/.gitmodules': '[submodule "tools/resources"]\n	path = tools/resources\n	url = https://github.com/w3c/testharness.js.git\n  ignore = dirty\n',
        }

        FAKE_FILES.update(FAKE_REPOSITORY)

        host = MockHost()
        host.executive = MockExecutive2()
        host.filesystem = MockFileSystem(files=FAKE_FILES)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '-d', 'w3c'], FAKE_FILES)

        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/csswg-tests/__init__.py'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/__init__.py'))
        self.assertTrue(fs.getsize('/mock-checkout/LayoutTests/w3c/web-platform-tests/__init__.py') > 0)

    def test_remove_obsolete_content(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/csswg-tests/temp': '',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/t/new.html': '<!doctype html><script src="/resources/testharness.js"></script><script src="/resources/testharnessreport.js"></script>',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/t/w3c-import.log': 'List of files:\n/LayoutTests/w3c/web-platform-tests/t/obsolete.html',
            '/mock-checkout/LayoutTests/w3c/web-platform-tests/t/obsolete.html': 'obsoleted content',
            '/mock-checkout/LayoutTests/w3c/web-platform-tests/t/obsolete-expected.txt': 'PASS',
        }

        FAKE_FILES.update(FAKE_REPOSITORY)

        host = MockHost()
        host.executive = MockExecutive2()
        host.filesystem = MockFileSystem(files=FAKE_FILES)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '-d', 'w3c'], FAKE_FILES)

        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/obsolete.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/obsolete-expected.txt'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/new.html'))

    def test_manual_slow_test(self):
        tests_options = '{"a": ["slow"]}'
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/csswg-tests/temp': '',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/t/new-manual.html': '<!doctype html><meta name="timeout" content="long"><script src="/resources/testharness.js"></script><script src="/resources/testharnessreport.js"></script>',
            '/mock-checkout/LayoutTests/tests-options.json': tests_options}
        FAKE_FILES.update(FAKE_REPOSITORY)

        host = MockHost()
        host.executive = MockExecutive2()
        host.filesystem = MockFileSystem(files=FAKE_FILES)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '-d', 'w3c'], FAKE_FILES)
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/new-manual.html'))
        self.assertEquals(tests_options, fs.read_text_file('/mock-checkout/LayoutTests/tests-options.json'))

    def test_webkit_test_runner_options(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/csswg-tests/t/test.html': '<!doctype html><script src="/resources/testharness.js"></script><script src="/resources/testharnessreport.js"></script>',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/css/test.html': '<!doctype html>\n<script src="/resources/testharness.js"></script><script src="/resources/testharnessreport.js"></script>',
            '/mock-checkout/LayoutTests/w3c/web-platform-tests/css/test.html': '<!-- doctype html --><!-- webkit-test-runner [ dummy ] -->',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/t/test.html': '<!doctype html><script src="/resources/testharness.js"></script><script src="/resources/testharnessreport.js"></script>',
            '/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.html': '<!-- doctype html --><!-- webkit-test-runner [ dummy ] -->',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/t/test.any.js': 'test(() => {}, "empty")',
            '/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.any.html': '<!-- This file is required for WebKit test infrastructure to run the templated test --><!-- webkit-test-runner [ dummy ] -->',
            '/mock-checkout/Source/WebCore/css/CSSProperties.json': '',
            '/mock-checkout/Source/WebCore/css/CSSValueKeywords.in': '',
        }
        FAKE_FILES.update(FAKE_REPOSITORY)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '-d', 'w3c'], FAKE_FILES)

        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/test.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.any.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.any.worker.html'))
        self.assertTrue('<!-- webkit-test-runner [ dummy ] -->' in fs.read_text_file('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/test.html').split('\n')[0])
        self.assertTrue('<!-- webkit-test-runner [ dummy ] -->' in fs.read_text_file('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.html').split('\n')[0])
        self.assertTrue('<!-- webkit-test-runner [ dummy ] -->' in fs.read_text_file('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.any.html').split('\n')[0])
        self.assertFalse('<!-- webkit-test-runner [ dummy ] -->' in fs.read_text_file('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.any.worker.html').split('\n')[0])

    def test_webkit_test_runner_import_reftests_with_absolute_paths_download(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/csswg-tests/t/test1.html': '<html><head><link rel=match href=/t/test1-ref.html></head></html>',
            '/mock-checkout/WebKitBuild/w3c-tests/csswg-tests/t/test1-ref.html': '<html></html>',
            '/mock-checkout/WebKitBuild/w3c-tests/csswg-tests/t/test2.html': '<html><head><link rel=match href=/some/directory/in/csswg-root/test2-ref.html></head></html>',
            '/mock-checkout/WebKitBuild/w3c-tests/csswg-tests/some/directory/in/csswg-root/test2-ref.html': '<html></html>',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/css/css-images/test3.html': '<html><head><link rel=match href=/css/css-images/test3-ref.html></head></html>',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/css/css-images/test3-ref.html': '<html></html>',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/css/css-images/test4.html': '<html><head><link rel=match href=/some/directory/in/wpt-root/test4-ref.html></head></html>',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/some/directory/in/wpt-root/test4-ref.html': '<html></html>',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/css/css-images/test5.html': '<html><head><link rel=match href="     /some/directory/in/wpt-root/test5-ref.html    "></head></html>',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/some/directory/in/wpt-root/test5-ref.html': '<html></html>',
        }
        FAKE_FILES.update(FAKE_REPOSITORY)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '-d', 'w3c'], FAKE_FILES)
        # test1
        self.assertTrue(fs.exists('/mock-checkout/WebKitBuild/w3c-tests/to-be-imported/csswg-tests/t/test1.html'))
        self.assertTrue(fs.exists('/mock-checkout/WebKitBuild/w3c-tests/to-be-imported/csswg-tests/t/test1-ref.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/csswg-tests/t/test1.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/csswg-tests/t/test1-expected.html'))
        # test2
        self.assertTrue(fs.exists('/mock-checkout/WebKitBuild/w3c-tests/to-be-imported/csswg-tests/t/test2.html'))
        self.assertTrue(fs.exists('/mock-checkout/WebKitBuild/w3c-tests/to-be-imported/csswg-tests/some/directory/in/csswg-root/test2-ref.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/csswg-tests/t/test2.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/csswg-tests/t/test2-expected.html'))
        # test3
        self.assertTrue(fs.exists('/mock-checkout/WebKitBuild/w3c-tests/to-be-imported/web-platform-tests/css/css-images/test3.html'))
        self.assertTrue(fs.exists('/mock-checkout/WebKitBuild/w3c-tests/to-be-imported/web-platform-tests/css/css-images/test3-ref.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-images/test3.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-images/test3-expected.html'))
        # test4
        self.assertTrue(fs.exists('/mock-checkout/WebKitBuild/w3c-tests/to-be-imported/web-platform-tests/css/css-images/test4.html'))
        self.assertTrue(fs.exists('/mock-checkout/WebKitBuild/w3c-tests/to-be-imported/web-platform-tests/some/directory/in/wpt-root/test4-ref.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-images/test4.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-images/test4-expected.html'))
        # test5
        self.assertTrue(fs.exists('/mock-checkout/WebKitBuild/w3c-tests/to-be-imported/web-platform-tests/css/css-images/test5.html'))
        self.assertTrue(fs.exists('/mock-checkout/WebKitBuild/w3c-tests/to-be-imported/web-platform-tests/some/directory/in/wpt-root/test5-ref.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-images/test5.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-images/test5-expected.html'))

    def test_webkit_test_runner_import_reftests_with_absolute_paths_from_source_dir(self):
        FAKE_FILES = {
            '/home/user/wpt/css/css-images/test1.html': '<html><head><link rel=match href=/css/css-images/test1-ref.html></head></html>',
            '/home/user/wpt/css/css-images/test1-ref.html': '<html></html>',
            '/home/user/wpt/css/css-images/test2.html': '<html><head><link rel=match href=/some/directory/in/wpt-root/test2-ref.html></head></html>',
            '/home/user/wpt/some/directory/in/wpt-root/test2-ref.html': '<html></html>',
            '/home/user/wpt/css/css-images/test3.html': '<html><head><link rel=match href="             /some/directory/in/wpt-root/test3-ref.html    "></head></html>',
            '/home/user/wpt/some/directory/in/wpt-root/test3-ref.html': '<html></html>',
        }
        FAKE_FILES.update(FAKE_REPOSITORY)

        fs = self.import_directory(['-s', '/home/user/wpt', '-d', '/mock-checkout/LayoutTests/w3c/web-platform-tests'], FAKE_FILES, 'css/css-images')
        # test1
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-images/test1.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-images/test1-expected.html'))
        # test2
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-images/test2.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-images/test2-expected.html'))
        # test3
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-images/test3.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-images/test3-expected.html'))

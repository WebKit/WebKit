# Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
# Copyright (C) 2020 Apple Inc. All rights reserved.
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

import json
import os
import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.executive_mock import MockExecutive2, ScriptError
from webkitpy.port.test import TestPort
from webkitpy.w3c.test_downloader import TestDownloader
from webkitpy.w3c.test_importer import parse_args, TestImporter

from webkitcorepy import OutputCapture

FAKE_SOURCE_DIR = '/tests/csswg'
FAKE_TEST_PATH = 'css-fake-1'

FAKE_FILES = {
    '/tests/csswg/css-fake-1/empty_dir/README.txt': '',
    '/mock-checkout/LayoutTests/w3c/css-fake-1/README.txt': '',
}

FAKE_RESOURCES = {
    '/mock-checkout/LayoutTests/imported/w3c/resources/import-expectations.json': '''
{
    "test1": "import",
    "test2": "skip"
}''',
    '/mock-checkout/LayoutTests/imported/w3c/resources/resource-files.json': '{"directories": [], "files": []}',
}

FAKE_FILES.update(FAKE_RESOURCES)


MINIMAL_TESTHARNESS = '''
<!doctype html>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
'''


class TestImporterTest(unittest.TestCase):
    def _parse_options(self, args):
        options, args = parse_args(args)
        return options

    def test_import_dir_with_no_tests_and_no_hg(self):
        host = MockHost()
        host.executive = MockExecutive2(exception=OSError())
        port = host.port_factory.get()
        fs = host.filesystem
        for path, contents in FAKE_FILES.items():
            fs.write_binary_file(path, contents)

        importer = TestImporter(port, [FAKE_TEST_PATH], self._parse_options(['-n', '-d', 'w3c', '-s', FAKE_SOURCE_DIR]))

        with OutputCapture():
            importer.do_import()

    def test_import_dir_with_no_tests(self):
        host = MockHost()
        host.executive = MockExecutive2(exception=ScriptError("abort: no repository found in '/Volumes/Source/src/wk/Tools/Scripts/webkitpy/w3c' (.hg not found)!"))
        port = host.port_factory.get()
        fs = host.filesystem
        for path, contents in FAKE_FILES.items():
            fs.write_binary_file(path, contents)

        importer = TestImporter(port, [FAKE_TEST_PATH], self._parse_options(['-n', '-d', 'w3c', '-s', FAKE_SOURCE_DIR]))
        with OutputCapture():
            importer.do_import()

    def test_import_dir_with_empty_init_py(self):
        FAKE_FILES = {
            '/tests/csswg/web-platform-tests/test1/__init__.py': '',
            '/tests/csswg/web-platform-tests/test2/__init__.py': 'NOTEMPTY',
        }
        FAKE_FILES.update(FAKE_RESOURCES)

        host = MockHost()
        port = host.port_factory.get()
        fs = host.filesystem
        for path, contents in FAKE_FILES.items():
            fs.write_binary_file(path, contents)

        importer = TestImporter(port, ['web-platform-tests/test1', 'web-platform-tests/test2'], self._parse_options(['-n', '-d', 'w3c', '-s', FAKE_SOURCE_DIR]))
        importer.do_import()

        self.assertTrue(host.filesystem.exists("/mock-checkout/LayoutTests/w3c/web-platform-tests/test1/__init__.py"))
        self.assertTrue(host.filesystem.exists("/mock-checkout/LayoutTests/w3c/web-platform-tests/test2/__init__.py"))
        self.assertTrue(host.filesystem.getsize("/mock-checkout/LayoutTests/w3c/web-platform-tests/test1/__init__.py") > 0)

    def import_directory(self, args, files, test_paths):
        host = MockHost()
        port = host.port_factory.get()
        fs = host.filesystem
        for path, contents in files.items():
            fs.write_binary_file(path, contents)

        options, args = parse_args(args)
        importer = TestImporter(port, test_paths, options)
        importer.do_import()
        return host.filesystem

    def import_downloaded_tests(self, args, files, test_port=False):
        # files are passed as parameter as we cannot clone/fetch/checkout a repo in mock system.

        host = MockHost()
        if test_port:
            port = TestPort(host)
        else:
            port = host.port_factory.get()
        fs = host.filesystem
        for path, contents in files.items():
            fs.write_binary_file(path, contents)

        options, test_paths = parse_args(args)
        importer = TestImporter(port, test_paths, options)
        importer._test_downloader = TestDownloader(importer.tests_download_path, port, importer.options)
        importer.do_import()
        return host.filesystem

    def test_harnesslinks_conversion(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/csswg-tests/t/test.html': MINIMAL_TESTHARNESS,
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/css/t/test.html': MINIMAL_TESTHARNESS,
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/t/test.html': MINIMAL_TESTHARNESS,
            '/mock-checkout/Source/WebCore/css/CSSProperties.json': '',
            '/mock-checkout/Source/WebCore/css/CSSValueKeywords.in': '',
        }
        FAKE_FILES.update(FAKE_RESOURCES)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '--no-clean-dest-dir', '-d', 'w3c'], FAKE_FILES)

        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/t/test.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.html'))
        self.assertTrue('src="/resources/testharness.js"' in fs.read_text_file('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.html'))
        self.assertTrue('src="/resources/testharness.js"' in fs.read_text_file('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/t/test.html'))

    def test_skip_test_import_download(self):
        FAKE_FILES = {}
        FAKE_FILES.update(FAKE_RESOURCES)
        FAKE_FILES.update({
            '/mock-checkout/WebKitBuild/w3c-tests/streams-api/reference-implementation/web-platform-tests/test.html': MINIMAL_TESTHARNESS,
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
        })

        fs = self.import_downloaded_tests(['--no-fetch', '--no-clean-dest-dir', '-d', 'w3c'], FAKE_FILES)

        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/dir-to-skip/file-to-import.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/dir-to-skip/dir-to-import/test-to-import.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/dir-to-skip/dir-to-not-import/test-to-not-import.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/dir-to-skip/test-to-skip.html'))

    def test_skip_test_import_source(self):
        FAKE_FILES = {}
        FAKE_FILES.update(FAKE_RESOURCES)
        FAKE_FILES.update({
            '/home/user/wpt/streams-api/reference-implementation/web-platform-tests/test.html': MINIMAL_TESTHARNESS,
            '/mock-checkout/LayoutTests/imported/w3c/resources/import-expectations.json': '''
{
"web-platform-tests/dir-to-skip": "skip",
"web-platform-tests/dir-to-skip/dir-to-import": "import",
"web-platform-tests/dir-to-skip/file-to-import.html": "import"
}''',
            '/home/user/wpt/web-platform-tests/dir-to-skip/test-to-skip.html': 'to be skipped',
            '/home/user/wpt/web-platform-tests/dir-to-skip/dir-to-import/test-to-import.html': 'to be imported',
            '/home/user/wpt/web-platform-tests/dir-to-skip/dir-to-not-import/test-to-not-import.html': 'to be skipped',
            '/home/user/wpt/web-platform-tests/dir-to-skip/file-to-import.html': 'to be imported',
        })

        fs = self.import_downloaded_tests(['-s', '/home/user/wpt', '--no-fetch', '--no-clean-dest-dir', '-d', 'w3c'], FAKE_FILES)

        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/dir-to-skip/file-to-import.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/dir-to-skip/dir-to-import/test-to-import.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/dir-to-skip/dir-to-not-import/test-to-not-import.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/dir-to-skip/test-to-skip.html'))

    def test_no_implicit_skip_with_download(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/css/css-images/tools/test1.html': MINIMAL_TESTHARNESS,
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/css/css-images/support/test2.html': MINIMAL_TESTHARNESS,
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/css/css-images/work-in-progress/test3.html': MINIMAL_TESTHARNESS,
        }
        FAKE_FILES.update(FAKE_RESOURCES)

        fs = self.import_directory(['-d', '/mock-checkout/LayoutTests/w3c/web-platform-tests'], FAKE_FILES, ['web-platform-tests/css/css-images'])
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/tools/test1.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/support/test2.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/work-in-progress/test3.html'))

    def test_no_implicit_skip_with_source(self):
        FAKE_FILES = {
            '/home/user/wpt/web-platform-tests/css/css-images/tools/test1.html': MINIMAL_TESTHARNESS,
            '/home/user/wpt/web-platform-tests/css/css-images/support/test2.html': MINIMAL_TESTHARNESS,
            '/home/user/wpt/web-platform-tests/css/css-images/work-in-progress/test3.html': MINIMAL_TESTHARNESS,
        }
        FAKE_FILES.update(FAKE_RESOURCES)

        fs = self.import_directory(['-s', '/home/user/wpt', '-d', '/mock-checkout/LayoutTests/w3c/web-platform-tests'], FAKE_FILES, ['web-platform-tests/css/css-images'])
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/tools/test1.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/support/test2.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/work-in-progress/test3.html'))

    def test_skip_new_directories(self):
        FAKE_FILES = {}
        FAKE_FILES.update(FAKE_RESOURCES)
        FAKE_FILES.update(
            {
                "/mock-checkout/LayoutTests/imported/w3c/resources/import-expectations.json": """
{
"web-platform-tests/css": "skip-new-directories"
}""",
                "/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/css/css-viewport/computedStyle-zoom.html": MINIMAL_TESTHARNESS,
                "/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/css/css-viewport/zoom/scroll-top-test-with-zoom.html": MINIMAL_TESTHARNESS,
            }
        )

        fs = self.import_downloaded_tests(["--no-fetch", "--no-clean-dest-dir", "-d", "w3c"], FAKE_FILES)

        import_expectations = json.loads(
            fs.read_text_file(
                "/mock-checkout/LayoutTests/imported/w3c/resources/import-expectations.json"
            )
        )
        self.assertEqual(
            "skip", import_expectations["web-platform-tests/css/css-viewport"]
        )

        self.assertFalse(
            fs.exists(
                "/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-viewport/computedStyle-zoom.html"
            )
        )
        self.assertFalse(
            fs.exists(
                "/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-viewport/zoom/scroll-top-test-with-zoom.html"
            )
        )

    def test_checkout_directory(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild2/w3c-tests/web-platform-tests/existing-test.html': '',
            '/mock-checkout/WebKitBuild2/w3c-tests/csswg-tests/test.html': '1',
        }

        FAKE_FILES.update(FAKE_RESOURCES)

        os.environ['WEBKIT_OUTPUTDIR'] = '/mock-checkout/WebKitBuild2'
        try:
            fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '--no-clean-dest-dir', '-d', 'w3c'], FAKE_FILES)
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

        FAKE_FILES.update(FAKE_RESOURCES)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '-d', 'w3c'], FAKE_FILES)

        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/old-test.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/old-test-expected.txt'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/existing-test.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/existing-test-expected.txt'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/.gitattributes'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/.gitignore'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/.svn'))

    def test_update_slow_test(self):
        existing_resource_files = {
            "directories": [],
            "files": [],
        }
        existing_tests_options = {
            "imported/w3c/web-platform-tests/a/existing-test.html": ["slow"],
        }

        # Note that neither old/new copies of existing-test.html are marked as long timeout.
        FAKE_FILES = {
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/a/existing-test.html': MINIMAL_TESTHARNESS + '1',
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/a/existing-test-expected.txt': '2',
            '/mock-checkout/LayoutTests/imported/w3c/resources/resource-files.json': json.dumps(existing_resource_files),
            '/mock-checkout/LayoutTests/tests-options.json': json.dumps(existing_tests_options),
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/a/existing-test.html': MINIMAL_TESTHARNESS,
            '/mock-checkout/WebKitBuild/w3c-tests/csswg-tests/test.html': '-1',
        }

        FAKE_FILES.update(FAKE_RESOURCES)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '--no-clean-dest-dir'], FAKE_FILES)

        # 'slow' should remain in tests-options.json.
        tests_options = json.loads(fs.read_text_file('/mock-checkout/LayoutTests/tests-options.json'))
        self.assertIn("slow", tests_options["imported/w3c/web-platform-tests/a/existing-test.html"])

    def test_clean_directory_option_partial_import(self):
        existing_resource_files = {
            "directories": [],
            "files": [
                "web-platform-tests/a/old-support.html",
                "web-platform-tests/b/old-support.html",
                "web-platform-tests/b/existing-support.html",
            ],
        }
        existing_tests_options = {
            "imported/w3c/web-platform-tests/a/old-test.html": ["slow"],
            "imported/w3c/web-platform-tests/b/old-test.html": ["slow"],
            "imported/w3c/web-platform-tests/b/existing-test.html": ["slow"],
        }

        FAKE_FILES = {}
        FAKE_FILES.update(FAKE_RESOURCES)
        FAKE_FILES.update({
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/a/old-test.html': '1',
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/a/old-test-expected.txt': '2',
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/a/old-support.html': '3',
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/b/old-test.html': '4',
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/b/old-test-expected.txt': '5',
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/b/old-support.html': '6',
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/b/existing-test.html': '4',
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/b/existing-test-expected.txt': '5',
            '/mock-checkout/LayoutTests/imported/w3c/resources/resource-files.json': json.dumps(existing_resource_files),
            '/mock-checkout/LayoutTests/tests-options.json': json.dumps(existing_tests_options),
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/b/existing-test.html': MINIMAL_TESTHARNESS,
            '/mock-checkout/WebKitBuild/w3c-tests/csswg-tests/test.html': '-1',
        })

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', 'web-platform-tests/b'], FAKE_FILES)

        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/a/old-test.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/a/old-test-expected.txt'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/b/old-test.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/b/old-test-expected.txt'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/b/existing-test.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/b/existing-test-expected.txt'))

        resource_files = json.loads(fs.read_text_file('/mock-checkout/LayoutTests/imported/w3c/resources/resource-files.json'))
        self.assertIn("web-platform-tests/a/old-support.html", resource_files["files"])
        self.assertNotIn("web-platform-tests/b/old-support.html", resource_files["files"])

        tests_options = json.loads(fs.read_text_file('/mock-checkout/LayoutTests/tests-options.json'))
        self.assertIn("imported/w3c/web-platform-tests/a/old-test.html", tests_options)
        self.assertNotIn("imported/w3c/web-platform-tests/b/old-test.html", tests_options)

    def test_clean_directory_option_prefix_name(self):
        existing_resource_files = {
            "directories": [],
            "files": [
                "web-platform-tests/cssom-view/old-support.html",
                "web-platform-tests/cssom/old-support.html",
                "web-platform-tests/cssom/existing-support.html",
            ],
        }
        existing_tests_options = {
            "imported/w3c/web-platform-tests/cssom-view/old-test.html": ["slow"],
            "imported/w3c/web-platform-tests/cssom/old-test.html": ["slow"],
            "imported/w3c/web-platform-tests/cssom/existing-test.html": ["slow"],
        }

        FAKE_FILES = {}
        FAKE_FILES.update(FAKE_RESOURCES)
        FAKE_FILES.update({
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/cssom-view/old-test.html': '1',
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/cssom-view/old-test-expected.txt': '2',
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/cssom-view/old-support.html': '3',
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/cssom/old-test.html': '4',
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/cssom/old-test-expected.txt': '5',
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/cssom/old-support.html': '6',
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/cssom/existing-test.html': '4',
            '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/cssom/existing-test-expected.txt': '5',
            '/mock-checkout/LayoutTests/imported/w3c/resources/resource-files.json': json.dumps(existing_resource_files),
            '/mock-checkout/LayoutTests/tests-options.json': json.dumps(existing_tests_options),
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/cssom/existing-test.html': MINIMAL_TESTHARNESS,
            '/mock-checkout/WebKitBuild/w3c-tests/csswg-tests/test.html': '-1',
        })

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', 'web-platform-tests/cssom'], FAKE_FILES)

        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/cssom-view/old-test.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/cssom-view/old-test-expected.txt'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/cssom/old-test.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/cssom/old-test-expected.txt'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/cssom/existing-test.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/cssom/existing-test-expected.txt'))

        resource_files = json.loads(fs.read_text_file('/mock-checkout/LayoutTests/imported/w3c/resources/resource-files.json'))
        self.assertIn("web-platform-tests/cssom-view/old-support.html", resource_files["files"])
        self.assertNotIn("web-platform-tests/cssom/old-support.html", resource_files["files"])

        tests_options = json.loads(fs.read_text_file('/mock-checkout/LayoutTests/tests-options.json'))
        self.assertIn("imported/w3c/web-platform-tests/cssom-view/old-test.html", tests_options)
        self.assertNotIn("imported/w3c/web-platform-tests/cssom/old-test.html", tests_options)

    def test_initpy_generation(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/csswg-tests/.gitmodules': '[submodule "tools/resources"]\n	path = tools/resources\n	url = https://github.com/w3c/testharness.js.git\n  ignore = dirty\n',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/.gitmodules': '[submodule "tools/resources"]\n	path = tools/resources\n	url = https://github.com/w3c/testharness.js.git\n  ignore = dirty\n',
        }

        FAKE_FILES.update(FAKE_RESOURCES)

        host = MockHost()
        host.executive = MockExecutive2()
        host.filesystem = MockFileSystem(files=FAKE_FILES)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '--no-clean-dest-dir', '-d', 'w3c'], FAKE_FILES)

        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/csswg-tests/__init__.py'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/__init__.py'))
        self.assertTrue(fs.getsize('/mock-checkout/LayoutTests/w3c/web-platform-tests/__init__.py') > 0)

    def test_remove_obsolete_content(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/csswg-tests/temp': '',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/t/new.html': MINIMAL_TESTHARNESS,
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/t/w3c-import.log': 'List of files:\n/LayoutTests/w3c/web-platform-tests/t/obsolete.html',
            '/mock-checkout/LayoutTests/w3c/web-platform-tests/t/obsolete.html': 'obsoleted content',
            '/mock-checkout/LayoutTests/w3c/web-platform-tests/t/obsolete-expected.txt': 'PASS',
        }

        FAKE_FILES.update(FAKE_RESOURCES)

        host = MockHost()
        host.executive = MockExecutive2()
        host.filesystem = MockFileSystem(files=FAKE_FILES)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '--no-clean-dest-dir', '-d', 'w3c'], FAKE_FILES)

        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/obsolete.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/obsolete-expected.txt'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/new.html'))

    def test_manual_slow_test(self):
        tests_options = '{"a": ["slow"]}'
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/csswg-tests/temp': '',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/t/new-manual.html': '<!doctype html><meta name="timeout" content="long"><script src="/resources/testharness.js"></script><script src="/resources/testharnessreport.js"></script>',
            '/mock-checkout/LayoutTests/tests-options.json': tests_options}
        FAKE_FILES.update(FAKE_RESOURCES)

        host = MockHost()
        host.executive = MockExecutive2()
        host.filesystem = MockFileSystem(files=FAKE_FILES)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '--no-clean-dest-dir', '-d', 'w3c'], FAKE_FILES)
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/new-manual.html'))
        self.assertEqual(tests_options, fs.read_text_file('/mock-checkout/LayoutTests/tests-options.json'))

    def test_crash_test_with_resource_file(self):
        FAKE_FILES = {
            '/home/user/wpt/web-platform-tests/css/css-images/test-crash.html': '<!DOCTYPE html>',
            '/home/user/wpt/web-platform-tests/css/css-images/some-file.html': '<!DOCTYPE html>',
            '/home/user/wpt/web-platform-tests/css/css-images/resources/some-file.html': '<!DOCTYPE html>',
            '/mock-checkout/LayoutTests/imported/w3c/resources/resource-files.json': '{"directories": [], "files": []}',
        }
        FAKE_FILES.update(FAKE_RESOURCES)

        fs = self.import_directory(['-s', '/home/user/wpt', '-d', '/mock-checkout/LayoutTests/imported/w3c/web-platform-tests'], FAKE_FILES, ['web-platform-tests/css/css-images'])
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/web-platform-tests/css/css-images/test-crash.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/web-platform-tests/css/css-images/some-file.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/imported/w3c/web-platform-tests/web-platform-tests/css/css-images/resources/some-file.html'))

        self.assertEqual(fs.read_text_file('/mock-checkout/LayoutTests/imported/w3c/resources/resource-files.json'), """{
    "directories": [],
    "files": [
        "web-platform-tests/css/css-images/some-file.html"
    ]
}""")

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
        FAKE_FILES.update(FAKE_RESOURCES)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '--no-clean-dest-dir', '-d', 'w3c'], FAKE_FILES)

        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/test.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.any.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.any.worker.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.any.serviceworker.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.any.sharedworker.html'))
        self.assertTrue('<!-- webkit-test-runner [ dummy ] -->' in fs.read_text_file('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/test.html').split('\n')[0])
        self.assertTrue('<!-- webkit-test-runner [ dummy ] -->' in fs.read_text_file('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.html').split('\n')[0])
        self.assertTrue('<!-- webkit-test-runner [ dummy ] -->' in fs.read_text_file('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.any.html').split('\n')[0])
        self.assertFalse('<!-- webkit-test-runner [ dummy ] -->' in fs.read_text_file('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.any.worker.html').split('\n')[0])

    def test_webkit_test_runner_import_reftests_with_absolute_paths_download(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/css/css-images/test3.html': '<html><head><link rel=match href=/css/css-images/test3-ref.html></head></html>',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/css/css-images/test3-ref.html': '<html></html>',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/css/css-images/test4.html': '<html><head><link rel=match href=/some/directory/in/wpt-root/test4-ref.html></head></html>',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/some/directory/in/wpt-root/test4-ref.html': '<html></html>',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/css/css-images/test5.html': '<html><head><link rel=match href="     /some/directory/in/wpt-root/test5-ref.html    "></head></html>',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/some/directory/in/wpt-root/test5-ref.html': '<html></html>',
        }
        FAKE_FILES.update(FAKE_RESOURCES)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '--no-clean-dest-dir', '-d', 'w3c'], FAKE_FILES)
        # test3
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-images/test3.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-images/test3-expected.html'))
        # test4
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-images/test4.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-images/test4-expected.html'))
        # test5
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-images/test5.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/css/css-images/test5-expected.html'))

    def test_webkit_test_runner_import_reftests_with_absolute_paths_from_source_dir(self):
        FAKE_FILES = {
            '/home/user/wpt/web-platform-tests/css/css-images/test1.html': '<html><head><link rel=match href=/css/css-images/test1-ref.html></head></html>',
            '/home/user/wpt/web-platform-tests/css/css-images/test1-ref.html': '<html></html>',
            '/home/user/wpt/web-platform-tests/css/css-images/test2.html': '<html><head><link rel=match href=/some/directory/in/wpt-root/test2-ref.html></head></html>',
            '/home/user/wpt/some/directory/in/wpt-root/test2-ref.html': '<html></html>',
            '/home/user/wpt/web-platform-tests/css/css-images/test3.html': '<html><head><link rel=match href="             /some/directory/in/wpt-root/test3-ref.html    "></head></html>',
            '/home/user/wpt/some/directory/in/wpt-root/test3-ref.html': '<html></html>',
            '/home/user/wpt/web-platform-tests/css/css-images/test4.html': '<html><head><link rel=match href=/web-platform-tests/css/css-images/test1-ref.html></head></html>',
            '/home/user/wpt/web-platform-tests/css/css-images/test4-ref.html': '<html></html>',

        }
        FAKE_FILES.update(FAKE_RESOURCES)

        fs = self.import_directory(['-s', '/home/user/wpt', '-d', '/mock-checkout/LayoutTests/w3c/web-platform-tests'], FAKE_FILES, ['web-platform-tests/css/css-images'])
        # test1
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/test1.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/test1-expected.html'))
        # test2
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/test2.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/test2-expected.html'))
        # test3
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/test3.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/test3-expected.html'))
        # test4
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/test4.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/test4-expected.html'))

    def test_keep_original_reftest_reference(self):
        FAKE_FILES = {
            '/home/user/wpt/web-platform-tests/css/css-images/test1.html': '<html><head><link rel=match href=test1-ref.html></head></html>',
            '/home/user/wpt/web-platform-tests/css/css-images/test1-ref.html': '<html></html>',
            '/home/user/wpt/web-platform-tests/css/css-images/test2.html': '<html><head><link rel=mismatch href=test2-notref.html></head></html>',
            '/home/user/wpt/web-platform-tests/css/css-images/test2-notref.html': '<html></html>',
        }
        FAKE_FILES.update(FAKE_RESOURCES)

        fs = self.import_directory(['-s', '/home/user/wpt', '-d', '/mock-checkout/LayoutTests/w3c/web-platform-tests'], FAKE_FILES, ['web-platform-tests/css/css-images'])

        # test1
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/test1.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/test1-ref.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/test1-expected.html'))

        # test2
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/test2.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/test2-notref.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/web-platform-tests/css/css-images/test2-expected-mismatch.html'))

    def test_template_test(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/t/test.any.js': '// META: global=window,dedicatedworker,sharedworker,serviceworker\n',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/t/test2.any.js': '\n// META: global=dedicatedworker,serviceworker\n',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/t/test3.any.js': '\n// META: global=worker\n',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/t/test4.any.js': '\n// META: global=dedicatedworker\n',
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/t/test5.any.js': '\n// META: global=window,worker\n',
        }
        FAKE_FILES.update(FAKE_RESOURCES)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '--no-clean-dest-dir', '-d', 'w3c'], FAKE_FILES)

        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.any.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.any.worker.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.any.serviceworker.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test.any.sharedworker.html'))

        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test2.any.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test2.any.worker.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test2.any.serviceworker.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test2.any.sharedworker.html'))

        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test3.any.worker.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test3.any.serviceworker.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test3.any.sharedworker.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test3.any.html'))

        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test4.any.worker.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test4.any.sharedworker.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test4.any.serviceworker.html'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test4.any.html'))

        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test5.any.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test5.any.worker.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test5.any.serviceworker.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/test5.any.sharedworker.html'))

    def test_template_test_variant(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/t/variant.any.js': '// META: variant=?1-10\n// META: variant=?11-20',
        }
        FAKE_FILES.update(FAKE_RESOURCES)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '--no-clean-dest-dir', '-d', 'w3c'], FAKE_FILES)

        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/variant.any.html'))
        self.assertTrue('<!-- META: variant=?1-10 -->' in fs.read_text_file('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/variant.any.html'))
        self.assertTrue('<!-- META: variant=?11-20 -->' in fs.read_text_file('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/variant.any.html'))

    def test_template_test_variant_dangling(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/t/variant.any.js': '// META: variant=?1-10\n// META: variant=?11-20',
            '/mock-checkout/LayoutTests/w3c/web-platform-tests/t/variant.any.js': '// META: variant=?1-10\n// META: variant=?11-20',
            '/mock-checkout/LayoutTests/w3c/web-platform-tests/t/variant.any_1-10-expected.txt': '1',
            '/mock-checkout/LayoutTests/w3c/web-platform-tests/t/variant.any_11-20-expected.txt': '2',
            '/mock-checkout/LayoutTests/w3c/web-platform-tests/t/variant.any_21-30-expected.txt': '3',
        }
        FAKE_FILES.update(FAKE_RESOURCES)

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '--no-clean-dest-dir', '-d', 'w3c'], FAKE_FILES)

        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/variant.any.html'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/variant.any_1-10-expected.txt'))
        self.assertTrue(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/variant.any_11-20-expected.txt'))
        self.assertFalse(fs.exists('/mock-checkout/LayoutTests/w3c/web-platform-tests/t/variant.any_21-30-expected.txt'))

    def test_non_dangling_platform(self):
        FAKE_FILES = {
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests/t/test.html': MINIMAL_TESTHARNESS,
            '/test.checkout/LayoutTests/imported/w3c/resources/import-expectations.json': '{}',
            '/test.checkout/LayoutTests/w3c/web-platform-tests/t/test.html': MINIMAL_TESTHARNESS,
            '/test.checkout/LayoutTests/w3c/web-platform-tests/t/test-expected.txt': '1',
            '/test.checkout/LayoutTests/platform/test-mac-leopard/w3c/web-platform-tests/t/test-expected.txt': '2',
            '/test.checkout/LayoutTests/platform/test-linux-x86_64/w3c/web-platform-tests/t/test-expected.txt': '3',
            '/test.checkout/LayoutTests/platform/unknown-platform/w3c/web-platform-tests/t/test-expected.txt': '4',
        }

        fs = self.import_downloaded_tests(['--no-fetch', '--import-all', '-d', 'w3c'], FAKE_FILES, test_port=True)

        self.assertTrue(fs.exists('/test.checkout/LayoutTests/w3c/web-platform-tests/t/test.html'))
        self.assertTrue(fs.exists('/test.checkout/LayoutTests/w3c/web-platform-tests/t/test-expected.txt'))
        self.assertTrue(fs.exists('/test.checkout/LayoutTests/platform/test-mac-leopard/w3c/web-platform-tests/t/test-expected.txt'))
        self.assertTrue(fs.exists('/test.checkout/LayoutTests/platform/test-linux-x86_64/w3c/web-platform-tests/t/test-expected.txt'))
        self.assertTrue(fs.exists('/test.checkout/LayoutTests/platform/unknown-platform/w3c/web-platform-tests/t/test-expected.txt'))

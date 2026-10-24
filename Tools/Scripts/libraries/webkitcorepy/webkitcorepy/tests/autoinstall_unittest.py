# Copyright (C) 2024 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import io
import os
import shutil
import tempfile
import unittest
import unittest.mock
from unittest.mock import patch
from urllib.error import URLError

from webkitcorepy import autoinstall, OutputCapture
from webkitcorepy.autoinstall import AutoInstall, Package, _default_pypi_indices, _pypi_indices_from_file
from webkitcorepy.version import Version


class DefaultPyPIIndexTest(unittest.TestCase):
    def test_no_config(self):
        with patch('os.path.isfile', return_value=False):
            self.assertEqual(_default_pypi_indices(), ['pypi.org'])

    def test_primary_index_only(self):
        config = io.StringIO('[global]\nindex-url = https://internal.example.com/\n')
        self.assertEqual(_pypi_indices_from_file(config), ['internal.example.com'])

    def test_extra_index_only(self):
        config = io.StringIO('[global]\nextra-index-url = https://extra.example.com/\n')
        self.assertEqual(_pypi_indices_from_file(config), ['extra.example.com'])

    def test_primary_and_extra_index(self):
        config = io.StringIO('[global]\nindex-url = https://primary.example.com/\nextra-index-url = https://extra.example.com/\n')
        result = _pypi_indices_from_file(config)
        self.assertEqual(result[0], 'primary.example.com')
        self.assertEqual(result[-1], 'extra.example.com')

    def test_multiple_extra_indexes(self):
        config = io.StringIO('[global]\nindex-url = https://primary.example.com/\nextra-index-url =\n    https://extra1.example.com/\n    https://extra2.example.com/\n')
        result = _pypi_indices_from_file(config)
        self.assertEqual(result, ['primary.example.com', 'extra1.example.com', 'extra2.example.com'])

    def test_index_with_simple_suffix(self):
        config = io.StringIO('[global]\nindex-url = https://internal.example.com/simple/\n')
        self.assertEqual(_pypi_indices_from_file(config), ['internal.example.com'])

    def test_index_with_path_prefix(self):
        config = io.StringIO('[global]\nindex-url = https://internal.example.com/pypi/simple/\n')
        self.assertEqual(_pypi_indices_from_file(config), ['internal.example.com/pypi'])

    def test_index_with_path_prefix_without_simple_suffix(self):
        config = io.StringIO('[global]\nindex-url = https://internal.example.com/pypi\n')
        self.assertEqual(_pypi_indices_from_file(config), ['internal.example.com/pypi'])

    def test_index_with_port(self):
        config = io.StringIO('[global]\nindex-url = http://localhost:8080/pypi/simple/\n')
        self.assertEqual(_pypi_indices_from_file(config), ['localhost:8080/pypi'])

    def test_index_with_credentials(self):
        config = io.StringIO('[global]\nindex-url = https://user:pass@internal.example.com/pypi/simple/\n')
        self.assertEqual(_pypi_indices_from_file(config), ['internal.example.com/pypi'])


class ArchiveTest(unittest.TestCase):
    @patch.object(AutoInstall, "_verify_index", autospec=True, return_value=None)
    @patch.object(
        autoinstall, "urlopen", autospec=True, side_effect=URLError("no network")
    )
    @patch.object(AutoInstall, "times_to_retry", new=3)
    def test_retry(self, mock_urlopen, mock_verify_index):
        with OutputCapture():
            archive = Package.Archive(
                "dummy", "http://example.example/dummy-1.0-py3-none-any.whl", Version(1, 0)
            )
            with self.assertRaises(URLError):
                archive.download()
            self.assertEqual(mock_urlopen.call_count, 4)


class IsCachedTest(unittest.TestCase):
    def setUp(self):
        self.temp_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_directory.cleanup)

        self.directory_patch = patch.object(AutoInstall, 'directory', self.temp_directory.name)
        self.manifest_patch = patch.object(AutoInstall, 'manifest', {})

        self.directory_patch.start()
        self.manifest_patch.start()

        self.addCleanup(self.directory_patch.stop)
        self.addCleanup(self.manifest_patch.stop)

    def test_is_cached_rejects_loose_version_match(self):
        package = Package('example', version=Version(1, 2, 3))
        AutoInstall.manifest[package.name] = {
            'index': AutoInstall.index,
            'version': '1.2',
        }
        self.assertFalse(package.is_cached())

    def test_is_cached_accepts_exact_version_match(self):
        package = Package('example', version=Version(1, 2, 3))
        AutoInstall.manifest[package.name] = {
            'index': AutoInstall.index,
            'version': '1.2.3',
        }
        self.assertTrue(package.is_cached())


class RemoveMetadataOfOtherVersionsTest(unittest.TestCase):
    def setUp(self):
        self.temp_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_directory.cleanup)

        self.directory_patch = patch.object(AutoInstall, 'directory', self.temp_directory.name)
        self.manifest_patch = patch.object(AutoInstall, 'manifest', {})

        self.directory_patch.start()
        self.manifest_patch.start()

        self.addCleanup(self.directory_patch.stop)
        self.addCleanup(self.manifest_patch.stop)

    def create(self, *entries):
        for entry in entries:
            path = os.path.join(self.temp_directory.name, entry)
            if entry.endswith('/'):
                os.makedirs(path)
            else:
                with open(path, 'w'):
                    pass

    def entries(self):
        return sorted(os.listdir(self.temp_directory.name))

    def install(self, package, version):
        AutoInstall.manifest[package.name] = {'index': AutoInstall.index, 'version': version}

    def test_is_cached_removes_metadata_of_other_versions(self):
        self.create('pyfakefs/', 'pyfakefs-5.7.3.dist-info/', 'pyfakefs-5.10.2.dist-info/')
        package = Package('pyfakefs', Version(5, 10, 2))
        self.install(package, '5.10.2')
        with OutputCapture():
            self.assertTrue(package.is_cached())
        self.assertEqual(self.entries(), ['pyfakefs', 'pyfakefs-5.10.2.dist-info'])

    def test_normalized_distribution_names(self):
        self.create(
            'pytest_asyncio-0.18.0.dist-info/', 'pytest_asyncio-0.19.0.dist-info/',
            'PyYAML-5.4.dist-info/', 'pyyaml-6.0.dist-info/',
            'pytest_asyncio_extra-0.18.0.dist-info/',
        )
        with OutputCapture():
            Package('pytest_asyncio', Version(0, 19, 0), pypi_name='pytest-asyncio').remove_metadata_of_other_versions('0.19.0')
            Package('yaml', Version(6, 0), pypi_name='PyYAML').remove_metadata_of_other_versions('6.0')
        self.assertEqual(self.entries(), ['pytest_asyncio-0.19.0.dist-info', 'pytest_asyncio_extra-0.18.0.dist-info', 'pyyaml-6.0.dist-info'])

    def test_keeps_metadata_without_metadata_of_installed_version(self):
        self.create('example-1.0.dist-info/', 'example-2.0.dist-info/')
        Package('example', Version(3, 0)).remove_metadata_of_other_versions('3.0')
        self.assertEqual(self.entries(), ['example-1.0.dist-info', 'example-2.0.dist-info'])

    def test_keeps_metadata_of_versions_that_cannot_be_compared(self):
        self.create('example-1.0.dist-info/', 'example-2.0rc1.dist-info/')
        Package('example', Version(1, 0)).remove_metadata_of_other_versions('1.0')
        self.assertEqual(self.entries(), ['example-1.0.dist-info', 'example-2.0rc1.dist-info'])

    def test_versions_are_compared_as_versions(self):
        # The manifest has versions without trailing zeros.
        self.create('example-1.0.0.dist-info/', 'example-0.9.dist-info/')
        with OutputCapture():
            Package('example', Version(1, 0)).remove_metadata_of_other_versions('1')
        self.assertEqual(self.entries(), ['example-1.0.0.dist-info'])

    def test_egg_info(self):
        self.create('example-1.0-py3.9.egg-info/', 'example-2.0-py3.9.egg-info/', 'example-0.5.egg-info')
        with OutputCapture():
            Package('example', Version(2, 0)).remove_metadata_of_other_versions('2.0')
        self.assertEqual(self.entries(), ['example-2.0-py3.9.egg-info'])

    def test_is_cached_checks_once(self):
        package = Package('example', Version(2, 0))
        self.install(package, '2.0')
        self.create('example-2.0.dist-info/')
        self.assertTrue(package.is_cached())
        self.create('example-1.0.dist-info/')
        self.assertTrue(package.is_cached())
        self.assertEqual(self.entries(), ['example-1.0.dist-info', 'example-2.0.dist-info'])


class MergeMoveTest(unittest.TestCase):
    # Package._merge_move is what lets multiple distributions contribute to a
    # shared namespace package (e.g. 'backports') without clobbering each other.
    def setUp(self):
        self.root = tempfile.mkdtemp()
        self.destination = os.path.join(self.root, 'site')
        os.makedirs(self.destination)

    def tearDown(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def _unpack(self, name, files):
        # Emulate one wheel install: build an unpacked wheel and merge each of
        # its top-level entries into the destination, as Package.install does.
        source = os.path.join(self.root, 'unpack-{}'.format(name))
        for relative in files:
            path = os.path.join(source, relative)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w') as handle:
                handle.write(relative)
        for top in os.listdir(source):
            Package._merge_move(os.path.join(source, top), os.path.join(self.destination, top))

    def _contents(self):
        return sorted(
            os.path.relpath(os.path.join(dirpath, name), self.destination).replace(os.sep, '/')
            for dirpath, _, filenames in os.walk(self.destination)
            for name in filenames
        )

    def test_install_order_does_not_matter(self):
        expected = ['sample/a.py', 'sample/b/__init__.py']

        self._unpack('sample-a', ['sample/a.py'])
        self._unpack('sample-b', ['sample/b/__init__.py'])
        self.assertEqual(self._contents(), expected)

        shutil.rmtree(self.destination)
        os.makedirs(self.destination)

        self._unpack('sample-b', ['sample/b/__init__.py'])
        self._unpack('sample-a', ['sample/a.py'])
        self.assertEqual(self._contents(), expected)

    def test_file_is_replaced_by_directory(self):
        # A leaf that changes kind between distributions is replaced, not merged.
        self._unpack('provides-file', ['sample/thing'])
        self._unpack('provides-package', ['sample/thing/__init__.py'])
        self.assertEqual(self._contents(), ['sample/thing/__init__.py'])


class VerifyIndexTest(unittest.TestCase):
    def test_error_response_names_index(self):
        response = unittest.mock.MagicMock()
        response.code = 404
        with patch.object(AutoInstall, 'index', 'new.example.com'), \
                patch.object(AutoInstall, '_previous_index', 'old.example.com'), \
                patch.object(AutoInstall, '_previous_ca_cert_path', None), \
                patch.object(AutoInstall, '_fatal_check', True), \
                patch.object(AutoInstall, '_request', return_value=response):
            with self.assertRaises(ValueError) as context:
                AutoInstall._verify_index()
        self.assertEqual(
            str(context.exception),
            'Failed to set AutoInstall index to new.example.com, received 404 response when searching for simple/pip',
        )

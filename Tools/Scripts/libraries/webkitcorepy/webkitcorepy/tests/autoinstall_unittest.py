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

import hashlib
import http.client
import io
import os
import shutil
import tarfile
import tempfile
import threading
import unittest
import unittest.mock
import zipfile
from collections import defaultdict
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
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
        with OutputCapture(), tempfile.TemporaryDirectory() as directory, patch.object(AutoInstall, 'directory', directory):
            archive = Package.Archive(
                "dummy", "http://example.example/dummy-1.0-py3-none-any.whl", Version(1, 0)
            )
            with self.assertRaises(URLError):
                archive.download()
            self.assertEqual(mock_urlopen.call_count, 4)


def index_response(href, name):
    response = unittest.mock.MagicMock()
    response.code = 200
    response.read.return_value = '<a href="{}">{}</a>'.format(href, name).encode('UTF-8')
    return response


def zip_of(payload):
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, 'w') as archive:
        archive.writestr('dummy-1.0/data', payload)
    return buffer.getvalue()


def tar_gz_of(payload):
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode='w:gz') as archive:
        info = tarfile.TarInfo('dummy-1.0/data')
        info.size = len(payload)
        archive.addfile(info, io.BytesIO(payload))
    return buffer.getvalue()


class ArchiveDownloadTest(unittest.TestCase):
    PAYLOAD = bytes(range(256)) * 64
    CONTENT = {'zip': zip_of(PAYLOAD), 'tar.gz': tar_gz_of(PAYLOAD)}

    def setUp(self):
        self.temp_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_directory.cleanup)
        for patcher in (
            patch.object(AutoInstall, 'directory', self.temp_directory.name),
            patch.object(AutoInstall, '_verify_index', return_value=None),
            patch.object(AutoInstall, 'times_to_retry', new=2),
            patch.dict(os.environ, {'NO_PROXY': '127.0.0.1', 'no_proxy': '127.0.0.1'}),
        ):
            patcher.start()
            self.addCleanup(patcher.stop)

        self.requests = []
        self.short_responses = 0
        self.content = self.CONTENT['zip']
        self.content_length = '{length}'
        self.chunked = False
        test = self

        class Handler(BaseHTTPRequestHandler):
            protocol_version = 'HTTP/1.1'

            def do_GET(self):
                test.requests.append(self.path)
                body = test.content
                if len(test.requests) <= test.short_responses:
                    body = body[:len(body) // 2]
                self.send_response(200)
                if test.chunked:
                    self.send_header('Transfer-Encoding', 'chunked')
                elif test.content_length is not None:
                    self.send_header('Content-Length', test.content_length.format(length=len(test.content)))
                self.end_headers()
                if test.chunked:
                    self.wfile.write('{:x}\r\n'.format(len(test.content)).encode('ascii'))
                self.wfile.write(body)
                if test.chunked and body == test.content:
                    self.wfile.write(b'\r\n0\r\n\r\n')
                self.close_connection = True

            def log_message(self, format, *args):
                pass

        self.server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        thread = threading.Thread(target=self.server.serve_forever, kwargs=dict(poll_interval=0.01), daemon=True)
        thread.start()
        self.addCleanup(thread.join)
        self.addCleanup(self.server.server_close)
        self.addCleanup(self.server.shutdown)

    def archive(self, extension='zip', fragment=''):
        self.content = self.CONTENT[extension]
        name = 'dummy-1.0.{}'.format(extension)
        href = 'http://127.0.0.1:{}/{}{}'.format(self.server.server_address[1], name, fragment)
        with patch.object(AutoInstall, '_request', return_value=index_response(href, name)):
            return Package('dummy', Version(1, 0)).archives()[-1]

    def assertDownloaded(self, archive, requests):
        with open(archive.path, 'rb') as file:
            self.assertEqual(file.read(), self.content)
        self.assertEqual(os.listdir(self.temp_directory.name), [os.path.basename(archive.path)])
        self.assertEqual(len(self.requests), requests)

    def assertNotDownloaded(self, requests):
        self.assertEqual(os.listdir(self.temp_directory.name), [])
        self.assertEqual(len(self.requests), requests)

    def test_success(self):
        archive = self.archive(fragment='#sha256={}'.format(hashlib.sha256(self.CONTENT['zip']).hexdigest()))
        archive.download()
        self.assertDownloaded(archive, requests=1)

    def test_tar_gz_success(self):
        archive = self.archive(extension='tar.gz')
        archive.download()
        self.assertDownloaded(archive, requests=1)

    def test_connection_dropped_every_time(self):
        self.short_responses = 3
        archive = self.archive()
        with OutputCapture(), self.assertRaises(IOError):
            archive.download()
        self.assertNotDownloaded(requests=3)

    def test_connection_dropped_once(self):
        self.short_responses = 1
        archive = self.archive()
        with OutputCapture():
            archive.download()
        self.assertDownloaded(archive, requests=2)

    def test_chunked_connection_dropped_once(self):
        self.chunked = True
        self.short_responses = 1
        archive = self.archive()
        with OutputCapture():
            archive.download()
        self.assertDownloaded(archive, requests=2)

    def test_zip_dropped_once_without_content_length(self):
        self.content_length = None
        self.short_responses = 1
        archive = self.archive()
        with OutputCapture():
            archive.download()
        self.assertDownloaded(archive, requests=2)

    def test_tar_gz_dropped_once_without_content_length(self):
        self.content_length = None
        self.short_responses = 1
        archive = self.archive(extension='tar.gz')
        with OutputCapture():
            archive.download()
        self.assertDownloaded(archive, requests=2)

    def test_hashed_zip_dropped_once_without_content_length(self):
        self.content_length = None
        self.short_responses = 1
        archive = self.archive(fragment='#sha256={}'.format(hashlib.sha256(self.CONTENT['zip']).hexdigest()))
        with OutputCapture():
            archive.download()
        self.assertDownloaded(archive, requests=2)

    def test_duplicated_content_length(self):
        self.content_length = '{length}, {length}'
        self.short_responses = 3
        archive = self.archive()
        with OutputCapture(), self.assertRaisesRegex(IOError, r'Downloaded \d+ of {} bytes'.format(len(self.content))):
            archive.download()
        self.assertNotDownloaded(requests=3)

    def test_conflicting_content_length(self):
        self.content_length = '{length}, 1'
        archive = self.archive()
        with OutputCapture() as captured:
            archive.download()
        self.assertIn('Skipping size verification', captured.webkitcorepy.log.getvalue())
        self.assertDownloaded(archive, requests=1)

    def test_matching_hash_is_trusted(self):
        archive = self.archive(fragment='#sha256={}'.format(hashlib.sha256(b'not a zip file').hexdigest()))
        self.content = b'not a zip file'
        with OutputCapture():
            archive.download()
        self.assertDownloaded(archive, requests=1)

    def test_hash_mismatch(self):
        archive = self.archive(fragment='#sha256={}'.format(hashlib.sha256(b'other').hexdigest()))
        with OutputCapture(), self.assertRaisesRegex(ValueError, 'sha256 hash mismatch'):
            archive.download()
        self.assertNotDownloaded(requests=1)

    def test_corrupt_archive_hash_mismatch(self):
        archive = self.archive(fragment='#sha256={}'.format(hashlib.sha256(self.CONTENT['zip']).hexdigest()))
        self.content = b'not a zip file'
        with OutputCapture(), self.assertRaisesRegex(ValueError, 'sha256 hash mismatch'):
            archive.download()
        self.assertNotDownloaded(requests=1)

    def test_hash_mismatch_without_content_length(self):
        self.content_length = None
        archive = self.archive(fragment='#sha256={}'.format(hashlib.sha256(b'other').hexdigest()))
        with OutputCapture(), self.assertRaisesRegex(ValueError, 'sha256 hash mismatch'):
            archive.download()
        self.assertNotDownloaded(requests=1)


@patch.object(AutoInstall, '_verify_index', new=lambda: None)
class ArchivesTest(unittest.TestCase):
    LINK = 'https://files.example.com/example-1.0.0.tar.gz'
    SHA256 = hashlib.sha256(b'example').hexdigest()

    def archives_for(self, href):
        with patch.object(AutoInstall, '_request', return_value=index_response(href, 'example-1.0.0.tar.gz')):
            return Package('example', Version(1, 0, 0)).archives()

    def assertChecksum(self, archive, algorithm, hexdigest):
        self.assertEqual((archive.checksum.algorithm, archive.checksum.hexdigest), (algorithm, hexdigest))

    def test_uppercase_hash_fragment(self):
        archives = self.archives_for('{}#sha256={}'.format(self.LINK, self.SHA256.upper()))
        self.assertEqual(archives[0].link, self.LINK)
        self.assertChecksum(archives[0], 'sha256', self.SHA256)

    def test_hash_fragment_with_other_fields(self):
        archives = self.archives_for('{}#sha256={}&egg=example'.format(self.LINK, self.SHA256))
        self.assertChecksum(archives[0], 'sha256', self.SHA256)

    def test_no_hash_fragment(self):
        self.assertIsNone(self.archives_for(self.LINK)[0].checksum)

    def test_unusable_hash_fragments(self):
        for fragment in (
            'notahash=0123',
            'md5={}'.format('0' * 32),
            'shake_128={}'.format('0' * 32),
            'sha256={}'.format('z' * 64),
            'sha256={}'.format(self.SHA256[:32]),
        ):
            with self.subTest(fragment=fragment):
                self.assertIsNone(self.archives_for('{}#{}'.format(self.LINK, fragment))[0].checksum)

    def test_hash_algorithm_unavailable(self):
        with patch.object(autoinstall.hashlib, 'new', side_effect=ValueError('unsupported hash type sha256')):
            archives = self.archives_for('{}#sha256={}'.format(self.LINK, self.SHA256))
        self.assertIsNone(archives[0].checksum)

    @patch.object(AutoInstall, 'times_to_retry', new=2)
    def test_index_unreachable_every_time(self):
        errors = [URLError('attempt {}'.format(attempt)) for attempt in range(3)]
        with patch.object(AutoInstall, '_request', side_effect=errors) as mock_request, OutputCapture() as captured:
            with self.assertRaises(URLError) as context:
                Package('example', Version(1, 0, 0)).archives()
        self.assertIs(context.exception, errors[-1])
        self.assertEqual(mock_request.call_count, 3)
        self.assertIn('Failed to retrieve the index of example, retrying', captured.webkitcorepy.log.getvalue())

    def test_index_cut_once(self):
        cut = unittest.mock.MagicMock()
        cut.code = 200
        cut.read.side_effect = http.client.IncompleteRead(b'<a href=')
        responses = [cut, index_response(self.LINK, 'example-1.0.0.tar.gz')]
        with patch.object(AutoInstall, '_request', side_effect=responses) as mock_request, OutputCapture():
            archives = Package('example', Version(1, 0, 0)).archives()
        self.assertEqual([archive.link for archive in archives], [self.LINK])
        self.assertEqual(mock_request.call_count, 2)


class InstallTest(unittest.TestCase):
    def setUp(self):
        self.temp_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_directory.cleanup)
        for patcher in (
            patch.object(AutoInstall, 'directory', self.temp_directory.name),
            patch.object(AutoInstall, 'manifest', {}),
            patch.object(AutoInstall, 'packages', defaultdict(list)),
            patch.object(AutoInstall, 'install'),
        ):
            patcher.start()
            self.addCleanup(patcher.stop)

    def test_failed_install_removes_archive(self):
        archive = Package.Archive('example', 'https://files.example.com/example-1.0.0.zip', Version(1, 0, 0), extension='zip')

        def download():
            with open(archive.path, 'wb') as file:
                file.write(b'not a zip file')

        with patch.object(Package, 'archives', return_value=[archive]), \
                patch.object(archive, 'download', side_effect=download), \
                OutputCapture(), self.assertRaises(zipfile.BadZipFile):
            Package('example', Version(1, 0, 0)).install()
        self.assertFalse(os.path.exists(archive.path))

    def test_install_removes_archive(self):
        archive = Package.Archive('example', 'https://files.example.com/example-1.0.0-py3-none-any.whl', Version(1, 0, 0), extension='whl')

        def download():
            with zipfile.ZipFile(archive.path, 'w') as wheel:
                wheel.writestr('example/__init__.py', '')
                wheel.writestr('example-1.0.0.dist-info/METADATA', '')

        with patch.object(Package, 'archives', return_value=[archive]), \
                patch.object(archive, 'download', side_effect=download), \
                OutputCapture():
            Package('example', Version(1, 0, 0)).install()
        self.assertFalse(os.path.exists(archive.path))
        self.assertTrue(os.path.isfile(os.path.join(self.temp_directory.name, 'example', '__init__.py')))


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

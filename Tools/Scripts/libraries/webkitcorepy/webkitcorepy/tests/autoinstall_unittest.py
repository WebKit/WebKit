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

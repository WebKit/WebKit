# Copyright (C) 2026 John James Jacoby. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import importlib.machinery
import importlib.util
import os
import tempfile
import unittest
from unittest.mock import patch


SCRIPT_PATH = os.path.join(os.path.dirname(__file__), 'built-product-archive')
SPEC = importlib.util.spec_from_file_location(
    'built_product_archive',
    SCRIPT_PATH,
    loader=importlib.machinery.SourceFileLoader('built_product_archive', SCRIPT_PATH),
)
built_product_archive = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(built_product_archive)


class SwiftRuntimeBinDirectoryTest(unittest.TestCase):
    def test_reads_cmake_generated_path(self):
        with tempfile.TemporaryDirectory() as directory:
            with open(os.path.join(directory, 'swift-runtime-bin-directory.txt'), 'w', encoding='utf-8') as file:
                file.write('C:\\swift-runtime-p\N{LATIN SMALL LETTER A WITH DIAERESIS}th-goes-here\n')
            self.assertEqual(
                built_product_archive.swiftRuntimeBinDirectory(directory),
                'C:\\swift-runtime-p\N{LATIN SMALL LETTER A WITH DIAERESIS}th-goes-here',
            )

    def test_returns_none_without_cmake_generated_path(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertIsNone(built_product_archive.swiftRuntimeBinDirectory(directory))

    def test_rejects_empty_cmake_generated_path(self):
        with tempfile.TemporaryDirectory() as directory:
            with open(os.path.join(directory, 'swift-runtime-bin-directory.txt'), 'w', encoding='utf-8'):
                pass
            with self.assertRaisesRegex(RuntimeError, 'file is empty'):
                built_product_archive.swiftRuntimeBinDirectory(directory)


class CopySwiftRuntimeLibrariesTest(unittest.TestCase):
    def test_copies_only_dlls_from_runtime_bin_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            runtimeBinDirectory = os.path.join(directory, 'usr', 'bin')
            destination = os.path.join(directory, 'archive')
            os.makedirs(runtimeBinDirectory)
            os.makedirs(destination)

            for filename in ('swiftCore.dll', 'FoundationEssentials.DLL', 'README.txt'):
                with open(os.path.join(runtimeBinDirectory, filename), 'w') as file:
                    file.write(filename)

            built_product_archive.copySwiftRuntimeLibraries(runtimeBinDirectory, destination)

            self.assertEqual(sorted(os.listdir(destination)), ['FoundationEssentials.DLL', 'swiftCore.dll'])

    def test_accepts_identical_existing_library(self):
        with tempfile.TemporaryDirectory() as directory:
            runtimeBinDirectory = os.path.join(directory, 'runtime')
            destination = os.path.join(directory, 'archive')
            os.makedirs(runtimeBinDirectory)
            os.makedirs(destination)
            for parent in (runtimeBinDirectory, destination):
                with open(os.path.join(parent, 'swiftCore.dll'), 'w') as file:
                    file.write('swiftCore.dll')

            built_product_archive.copySwiftRuntimeLibraries(runtimeBinDirectory, destination)

            self.assertEqual(os.listdir(destination), ['swiftCore.dll'])

    def test_rejects_conflicting_existing_library(self):
        with tempfile.TemporaryDirectory() as directory:
            runtimeBinDirectory = os.path.join(directory, 'runtime')
            destination = os.path.join(directory, 'archive')
            os.makedirs(runtimeBinDirectory)
            os.makedirs(destination)
            with open(os.path.join(runtimeBinDirectory, 'swiftCore.dll'), 'w') as file:
                file.write('runtime')
            with open(os.path.join(destination, 'swiftCore.dll'), 'w') as file:
                file.write('existing')

            with self.assertRaisesRegex(RuntimeError, 'conflicts with existing file'):
                built_product_archive.copySwiftRuntimeLibraries(runtimeBinDirectory, destination)

    def test_fails_when_swift_runtime_is_unavailable(self):
        with tempfile.TemporaryDirectory() as directory:
            runtimeBinDirectory = os.path.join(directory, 'usr', 'bin')
            destination = os.path.join(directory, 'archive')
            os.makedirs(runtimeBinDirectory)
            os.makedirs(destination)

            with self.assertRaisesRegex(RuntimeError, 'Could not find the Swift runtime'):
                built_product_archive.copySwiftRuntimeLibraries(runtimeBinDirectory, destination)


class ArchiveBuiltProductTest(unittest.TestCase):
    def test_windows_archive_includes_swift_runtime_libraries(self):
        oldConfigurationBuildDirectory = built_product_archive._configurationBuildDirectory
        built_product_archive._configurationBuildDirectory = os.path.join('WebKitBuild', 'Release')
        try:
            with patch.object(built_product_archive, 'removeDirectoryIfExists'), patch.object(
                built_product_archive, 'copyBuildFiles'
            ), patch.object(
                built_product_archive, 'swiftRuntimeBinDirectory', return_value=os.path.join('Swift', 'Runtime')
            ), patch.object(
                built_product_archive, 'copySwiftRuntimeLibraries'
            ) as copyRuntime, patch.object(
                built_product_archive, 'createZip', return_value=0
            ), patch.object(built_product_archive.shutil, 'rmtree'):
                result = built_product_archive.archiveBuiltProduct('Release', 'win', 'win')
        finally:
            built_product_archive._configurationBuildDirectory = oldConfigurationBuildDirectory

        self.assertIsNone(result)
        copyRuntime.assert_called_once_with(
            os.path.join('Swift', 'Runtime'),
            os.path.join('WebKitBuild', 'Release', 'thin', 'bin'),
        )

    def test_windows_archive_without_swift_does_not_copy_runtime_libraries(self):
        oldConfigurationBuildDirectory = built_product_archive._configurationBuildDirectory
        built_product_archive._configurationBuildDirectory = os.path.join('WebKitBuild', 'Release')
        try:
            with patch.object(built_product_archive, 'removeDirectoryIfExists'), patch.object(
                built_product_archive, 'copyBuildFiles'
            ), patch.object(built_product_archive, 'swiftRuntimeBinDirectory', return_value=None), patch.object(
                built_product_archive, 'copySwiftRuntimeLibraries'
            ) as copyRuntime, patch.object(
                built_product_archive, 'createZip', return_value=0
            ), patch.object(built_product_archive.shutil, 'rmtree'):
                result = built_product_archive.archiveBuiltProduct('Release', 'win', 'win')
        finally:
            built_product_archive._configurationBuildDirectory = oldConfigurationBuildDirectory

        self.assertIsNone(result)
        copyRuntime.assert_not_called()


if __name__ == '__main__':
    unittest.main()

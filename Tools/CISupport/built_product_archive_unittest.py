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
import json
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


class SwiftRuntimeIsRequiredTest(unittest.TestCase):
    def test_detects_enabled_swift_features(self):
        configurations = (
            ('#define ENABLE_BACK_FORWARD_LIST_SWIFT 1\n#define ENABLE_SWIFT_DEMO_URI_SCHEME 0\n', True),
            ('#define ENABLE_BACK_FORWARD_LIST_SWIFT 0\n#define ENABLE_SWIFT_DEMO_URI_SCHEME 1\n', True),
            ('#define ENABLE_BACK_FORWARD_LIST_SWIFT 0\n#define ENABLE_SWIFT_DEMO_URI_SCHEME 0\n', False),
        )
        for contents, expected in configurations:
            with self.subTest(contents=contents), tempfile.TemporaryDirectory() as directory:
                with open(os.path.join(directory, 'cmakeconfig.h'), 'w') as file:
                    file.write(contents)
                self.assertEqual(built_product_archive.swiftRuntimeIsRequired(directory), expected)


class CopySwiftRuntimeLibrariesTest(unittest.TestCase):
    def test_copies_only_dlls_from_runtime_bin_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            compilerBinDirectory = os.path.join(directory, 'toolchain', 'usr', 'bin')
            runtimeBinDirectory = os.path.join(directory, 'usr', 'bin')
            destination = os.path.join(directory, 'archive')
            os.makedirs(compilerBinDirectory)
            os.makedirs(runtimeBinDirectory)
            os.makedirs(destination)

            with open(os.path.join(compilerBinDirectory, 'sourcekitdInProc.dll'), 'w') as file:
                file.write('sourcekitdInProc.dll')
            for filename in ('swiftCore.dll', 'FoundationEssentials.DLL', 'README.txt'):
                with open(os.path.join(runtimeBinDirectory, filename), 'w') as file:
                    file.write(filename)

            targetInfo = json.dumps({
                'paths': {'runtimeLibraryPaths': [compilerBinDirectory, runtimeBinDirectory]},
            })
            with patch.object(built_product_archive.shutil, 'which', return_value='swiftc'), patch.object(
                built_product_archive.subprocess, 'check_output', return_value=targetInfo
            ):
                built_product_archive.copySwiftRuntimeLibraries(destination)

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

            targetInfo = json.dumps({'paths': {'runtimeLibraryPaths': [runtimeBinDirectory]}})
            with patch.object(built_product_archive.shutil, 'which', return_value='swiftc'), patch.object(
                built_product_archive.subprocess, 'check_output', return_value=targetInfo
            ):
                built_product_archive.copySwiftRuntimeLibraries(destination)

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

            targetInfo = json.dumps({'paths': {'runtimeLibraryPaths': [runtimeBinDirectory]}})
            with patch.object(built_product_archive.shutil, 'which', return_value='swiftc'), patch.object(
                built_product_archive.subprocess, 'check_output', return_value=targetInfo
            ):
                with self.assertRaisesRegex(RuntimeError, 'conflicts with existing file'):
                    built_product_archive.copySwiftRuntimeLibraries(destination)

    def test_fails_when_swiftc_is_unavailable(self):
        with tempfile.TemporaryDirectory() as destination, patch.object(
            built_product_archive.shutil, 'which', return_value=None
        ):
            with self.assertRaisesRegex(RuntimeError, 'Could not find swiftc'):
                built_product_archive.copySwiftRuntimeLibraries(destination)

    def test_fails_when_runtime_bin_directory_is_unavailable(self):
        with tempfile.TemporaryDirectory() as directory:
            runtimeBinDirectory = os.path.join(directory, 'usr', 'bin')
            destination = os.path.join(directory, 'archive')
            os.makedirs(runtimeBinDirectory)
            os.makedirs(destination)

            targetInfo = json.dumps({'paths': {'runtimeLibraryPaths': [runtimeBinDirectory]}})
            with patch.object(built_product_archive.shutil, 'which', return_value='swiftc'), patch.object(
                built_product_archive.subprocess, 'check_output', return_value=targetInfo
            ):
                with self.assertRaisesRegex(RuntimeError, 'Could not find the Swift runtime bin directory'):
                    built_product_archive.copySwiftRuntimeLibraries(destination)


class ArchiveBuiltProductTest(unittest.TestCase):
    def test_windows_archive_includes_swift_runtime_libraries(self):
        oldConfigurationBuildDirectory = built_product_archive._configurationBuildDirectory
        built_product_archive._configurationBuildDirectory = os.path.join('WebKitBuild', 'Release')
        try:
            with patch.object(built_product_archive, 'removeDirectoryIfExists'), patch.object(
                built_product_archive, 'copyBuildFiles'
            ), patch.object(built_product_archive, 'swiftRuntimeIsRequired', return_value=True), patch.object(
                built_product_archive, 'copySwiftRuntimeLibraries'
            ) as copyRuntime, patch.object(
                built_product_archive, 'createZip', return_value=0
            ), patch.object(built_product_archive.shutil, 'rmtree'):
                result = built_product_archive.archiveBuiltProduct('Release', 'win', 'win')
        finally:
            built_product_archive._configurationBuildDirectory = oldConfigurationBuildDirectory

        self.assertIsNone(result)
        copyRuntime.assert_called_once_with(os.path.join('WebKitBuild', 'Release', 'thin', 'bin'))

    def test_windows_archive_without_swift_does_not_copy_runtime_libraries(self):
        oldConfigurationBuildDirectory = built_product_archive._configurationBuildDirectory
        built_product_archive._configurationBuildDirectory = os.path.join('WebKitBuild', 'Release')
        try:
            with patch.object(built_product_archive, 'removeDirectoryIfExists'), patch.object(
                built_product_archive, 'copyBuildFiles'
            ), patch.object(built_product_archive, 'swiftRuntimeIsRequired', return_value=False), patch.object(
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

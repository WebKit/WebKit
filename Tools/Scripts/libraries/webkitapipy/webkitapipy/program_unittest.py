# Copyright (C) 2025 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import contextlib
import tempfile
import subprocess
from io import StringIO
from pathlib import Path
from typing import NamedTuple
from unittest import TestCase
from unittest.mock import MagicMock, patch

from . import program


def create_sdk_dir(root: Path):
    sdk = root / 'iPhoneOS.sdk'
    sdk.mkdir()
    lib = sdk / 'usr/lib/libobjc.tbd'
    lib.parent.mkdir(parents=True)
    lib.touch()
    merged_lib = sdk / 'usr/lib/swift/libswiftQuickLook.dylib'
    merged_lib.parent.mkdir(parents=True)
    merged_lib.symlink_to('../../../System/Library/Frameworks/QuickLook.framework/QuickLook')
    return sdk


def create_sdkdb_dir(root: Path):
    sdkdb = root / 'iphoneos26'
    sdkdb.mkdir()
    partial = sdkdb / 'Foundation.partial.sdkdb'
    partial.touch()
    return sdkdb


def create_framework_and_dylib(root: Path):
    lib = root / 'libfoo.dylib'
    lib.touch()

    framework = root / 'foo.framework'
    framework.mkdir()
    (framework / 'foo').touch()
    (framework / 'foo.tbd').touch()
    return framework, lib


def create_local_sdkdb(root: Path):
    sdkdb_dir = root / program.FRAMEWORK_SDKDB_DIR
    sdkdb_dir.mkdir()
    partial_sdkdb = sdkdb_dir / 'foo.partial.sdkdb'
    partial_sdkdb.touch()
    return partial_sdkdb


class CLITest(TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.workdir = Path(self.tempdir.name)
        self.sdk_dir = create_sdk_dir(self.workdir)
        self.sdkdb_dir = create_sdkdb_dir(self.workdir)
        self.cache = self.workdir / 'sdkdb.sqlite3'
        self.framework, self.dylib = create_framework_and_dylib(self.workdir)
        self.local_sdkdb = create_local_sdkdb(self.workdir)

        self.base = ['--arch-name=arm64e', f'--sdk-dir={self.sdk_dir}',
                     f'--sdkdb-cache={self.cache}',
                     f'--sdkdb-dir={self.sdkdb_dir}',
                     f'-F{self.workdir}', f'-L{self.workdir}']

    class CallResult(NamedTuple):
        api_report: MagicMock
        sdkdb: MagicMock
        stdout: str
        stderr: str
        dependencies: list[str]

    def call(self, *args) -> CallResult:
        argv = self.base.copy()
        argv.extend(map(str, args))

        depfile = self.workdir / 'depfile.d'
        argv.append(f'--depfile={depfile}')

        with (contextlib.redirect_stderr(StringIO()) as stderr,
              contextlib.redirect_stdout(StringIO()) as stdout,
              patch('webkitapipy.program.SDKDB') as mock_sdkdb,
              patch('webkitapipy.program.APIReport') as mock_api):
            mock_sdkdb.return_value.stats.return_value = (1, 2, 3)
            program.main(argv)

        dependencies = (depfile.read_text().removeprefix('dependencies: ')
                        .rstrip().split(' \\\n  '))

        return self.CallResult(api_report=mock_api.return_value,
                               sdkdb=mock_sdkdb.return_value,
                               stdout=stdout.read(),
                               stderr=stderr.read(),
                               dependencies=dependencies)

    def test_loads_partial_sdkdb_for_framework(self):
        # When invoked with a ld-style framework argument:
        result = self.call(self.dylib, '-framework', 'foo')
        # It loads the framework binary...
        result.sdkdb.add_binary.assert_called_with(
            self.framework / 'foo', arch='arm64e'
        )
        # ...and the corresponding partial SDKDB.
        result.sdkdb.add_partial_sdkdb.assert_called_with(
            self.local_sdkdb, spi=True, abi=True
        )

    def test_loads_partial_sdkdb_for_main_file(self):
        # When invoked with a framework binary to analyze:
        result = self.call(self.framework / 'foo')
        # It finds and loads the corresponding partial SDKDB.
        result.sdkdb.add_partial_sdkdb.assert_called_with(
            self.local_sdkdb, spi=True, abi=True
        )
        # It works around rdar://153937150 by tracking the framework's tbd as
        # an input.
        self.assertIn(f'{self.framework}/foo.tbd', result.dependencies)

    def test_does_not_load_merged_swiftoverlays(self):
        result = self.call(self.framework / 'foo')
        self.assertNotIn(f'{self.sdk_dir}/usr/lib/swift/libswiftQuickLook.dylib', result.dependencies)

    def test_depends_on_primary_file(self):
        result = self.call(self.framework / 'foo')
        self.assertIn(f'{self.framework}/foo', result.dependencies)

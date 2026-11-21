# Copyright (C) 2026 Igalia S.L.
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
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import contextlib
import io
import shutil
import sys
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest import mock

from webkitpy import android_dependencies
from webkitpy.android_dependencies import NDK_REVISION, NDK_ROOT, Download


@unittest.skipUnless(sys.platform.startswith('linux'), 'Android builds are only supported on Linux')
class AndroidDependenciesTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, self.temporary_directory)
        self.directory = self.temporary_directory / 'DependenciesAndroid'
        self.archives_directory = self.temporary_directory / 'archives'
        self.archives_directory.mkdir()
        patcher = mock.patch.object(android_dependencies, 'can_download_ndk', return_value=True)
        patcher.start()
        self.addCleanup(patcher.stop)

    def create_file(self, path, contents=''):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents)
        return path

    def create_ndk(self, path, revision=NDK_REVISION):
        self.create_file(path / 'build' / 'cmake' / 'android.toolchain.cmake')
        self.create_file(path / 'source.properties', f'Pkg.Desc = Android NDK\nPkg.Revision = {revision}\n')
        return path

    def create_dependencies(self, path):
        (path / 'lib' / 'pkgconfig').mkdir(parents=True)
        return path

    def create_tarball(self, name, files):
        contents = Path(tempfile.mkdtemp(dir=self.temporary_directory))
        for file, file_contents in files.items():
            self.create_file(contents / file, file_contents)
        tarball = self.archives_directory / name
        with tarfile.open(tarball, 'w:xz') as archive:
            for entry in contents.iterdir():
                archive.add(entry, arcname=entry.name)
        return tarball

    def download_of(self, path):
        return Download(path.as_uri(), android_dependencies.sha256(path))

    def ndk_downloads(self):
        ndk = self.create_ndk(Path(tempfile.mkdtemp(dir=self.temporary_directory)) / NDK_ROOT)
        tarball = self.archives_directory / 'android-ndk-r29-linux.tar.xz'
        with tarfile.open(tarball, 'w:xz') as archive:
            archive.add(ndk, arcname=NDK_ROOT)
        return [self.download_of(tarball)]

    def dependencies_downloads(self, version='1.0-20261030A'):
        development = self.create_tarball(f'deps-android-arm64-{version}.tar.xz', {'lib/pkgconfig/deps.pc': ''})
        runtime = self.create_tarball(f'deps-android-arm64-{version}-runtime.tar.xz', {'lib/libdeps.so': ''})
        return {'arm64-v8a': [self.download_of(development), self.download_of(runtime)]}

    def update(self, environ=None, ndk_downloads=None, dependencies_downloads=None):
        # Keep the report of the downloads out of the test output.
        with contextlib.redirect_stdout(io.StringIO()):
            android_dependencies.update(
                self.directory, 'arm64-v8a', environ or {},
                ndk_downloads or self.ndk_downloads(), dependencies_downloads or self.dependencies_downloads())

    def linked(self, name):
        return (self.directory / name).resolve()

    def entries(self):
        return sorted(path.name for path in self.directory.iterdir())

    def test_download_name(self):
        self.assertEqual(Download('https://example.org/deps-android-arm64-1.0-20261030A.tar.xz', '').name, 'deps-android-arm64-1.0-20261030A')
        self.assertEqual(Download('https://example.org/android-ndk-r29-linux.zip', '').name, 'android-ndk-r29-linux')

    def test_dependencies_url(self):
        with mock.patch.multiple(android_dependencies, DEPENDENCIES_VERSION='1.0', DEPENDENCIES_UPLOAD='20261030A'):
            self.assertEqual(android_dependencies.dependencies_url('arm64'),
                             'https://wpewebkit.org/android/bootstrap/1.0/wpewebkit-deps-android-arm64-1.0-20261030A.tar.xz')
            self.assertEqual(android_dependencies.dependencies_url('x86_64', 'runtime'),
                             'https://wpewebkit.org/android/bootstrap/1.0/wpewebkit-deps-android-x86_64-1.0-runtime-20261030A.tar.xz')

    def test_every_archive_is_unpacked_once(self):
        ndk_downloads = self.ndk_downloads()
        dependencies_downloads = self.dependencies_downloads()
        self.update(ndk_downloads=ndk_downloads, dependencies_downloads=dependencies_downloads)
        dependencies = self.linked('dependencies-arm64-v8a')
        self.assertEqual(dependencies, self.directory / 'deps-android-arm64-1.0-20261030A')
        self.assertTrue((dependencies / 'lib' / 'pkgconfig' / 'deps.pc').is_file())
        self.assertTrue((dependencies / 'lib' / 'libdeps.so').is_file())

        # Without the archives, a second download would fail.
        shutil.rmtree(self.archives_directory)
        self.update(ndk_downloads=ndk_downloads, dependencies_downloads=dependencies_downloads)
        self.assertEqual(self.linked('dependencies-arm64-v8a'), dependencies)

    def test_archive_with_another_checksum(self):
        tarball = self.create_tarball('other-1.0.tar.xz', {'lib/pkgconfig/other.pc': ''})
        with self.assertRaisesRegex(SystemExit, r'^file:\S+ has the SHA-256 checksum [0-9a-f]{64} instead of 0{64}\.$'):
            self.update(dependencies_downloads={'arm64-v8a': [Download(tarball.as_uri(), '0' * 64)]})
        self.assertEqual(self.entries(), ['.lock', 'android-ndk-r29-linux'])

    @unittest.skipUnless(shutil.which('unzip'), 'unzip is not installed')
    def test_directory_of_a_zip_archive(self):
        archive = self.archives_directory / 'ndk-r29-linux.zip'
        with zipfile.ZipFile(archive, 'w') as zip_archive:
            zip_archive.writestr(f'{NDK_ROOT}/source.properties', f'Pkg.Revision = {NDK_REVISION}\n')
        self.directory.mkdir()
        with contextlib.redirect_stdout(io.StringIO()):
            path = android_dependencies.download([self.download_of(archive)], self.directory, root=NDK_ROOT)
        self.assertEqual(android_dependencies.ndk_revision(path), NDK_REVISION)

    def test_given_ndk_and_dependencies_are_not_downloaded(self):
        ndk = self.create_ndk(self.temporary_directory / 'ndk')
        dependencies = self.create_dependencies(self.temporary_directory / 'dependencies')
        nothing = [Download('file:///nonexistent/nothing.tar.xz', '')]
        self.update({'WEBKIT_ANDROID_NDK': str(ndk), 'WEBKIT_ANDROID_DEPENDENCIES': str(dependencies)}, nothing, {'arm64-v8a': nothing})
        self.assertEqual(self.linked('ndk'), ndk)
        self.assertEqual(self.linked('dependencies-arm64-v8a'), dependencies)
        self.assertEqual(self.entries(), ['.lock', 'dependencies-arm64-v8a', 'ndk'])

    def test_ndk_of_the_android_sdk(self):
        sdk = self.temporary_directory / 'sdk'
        self.create_ndk(sdk / 'ndk' / '27.0.12077973', '27.0.12077973')
        self.create_ndk(sdk / 'ndk' / '30.0.14904198', '30.0.14904198')
        self.update({'ANDROID_HOME': str(sdk)})
        self.assertEqual(self.linked('ndk'), self.directory / 'android-ndk-r29-linux', 'the other NDKs of the Android SDK are not used')

        ndk = self.create_ndk(sdk / 'ndk' / NDK_REVISION)
        self.update({'ANDROID_SDK_ROOT': str(sdk)})
        self.assertEqual(self.linked('ndk'), ndk, 'the NDK r29 of the Android SDK is used, even when a newer one is installed')

        given_ndk = self.create_ndk(self.temporary_directory / 'given-ndk')
        self.update({'ANDROID_HOME': str(sdk), 'WEBKIT_ANDROID_NDK': str(given_ndk)})
        self.assertEqual(self.linked('ndk'), given_ndk, 'WEBKIT_ANDROID_NDK is used before the NDK of the Android SDK')

    def test_another_ndk_gives_a_warning(self):
        ndk = self.create_ndk(self.temporary_directory / 'ndk', '27.0.12077973')
        with contextlib.redirect_stderr(io.StringIO()) as stderr:
            self.update({'WEBKIT_ANDROID_NDK': str(ndk)})
        self.assertRegex(stderr.getvalue(), r'^Warning: WEBKIT_ANDROID_NDK is the NDK 27\.0\.12077973')
        self.assertEqual(self.linked('ndk'), ndk)

    def test_relative_paths(self):
        for variable in ('WEBKIT_ANDROID_NDK', 'WEBKIT_ANDROID_DEPENDENCIES'):
            with self.assertRaisesRegex(SystemExit, rf"^{variable} is set to 'relative', which is not an absolute path\.$"):
                self.update({variable: 'relative'})

    def test_dependencies_without_pkgconfig_files(self):
        with self.assertRaisesRegex(SystemExit, r'has no lib/pkgconfig directory\.$'):
            self.update({'WEBKIT_ANDROID_DEPENDENCIES': str(self.temporary_directory)})

    def test_ndk_without_toolchain_file(self):
        ndk = self.create_ndk(self.temporary_directory / 'ndk')
        (ndk / 'build' / 'cmake' / 'android.toolchain.cmake').unlink()
        with self.assertRaisesRegex(SystemExit, r"android\.toolchain\.cmake' does not exist\.$"):
            self.update({'WEBKIT_ANDROID_NDK': str(ndk)})

    def test_ndk_is_downloaded_only_for_linux_on_x86_64(self):
        with mock.patch.object(android_dependencies, 'can_download_ndk', return_value=False):
            with self.assertRaisesRegex(SystemExit, r'^The Android NDK can only be downloaded for Linux on x86_64\.'):
                self.update()

    def test_other_versions_are_kept(self):
        self.update(dependencies_downloads=self.dependencies_downloads('1.0-20261030A'))
        first = self.linked('dependencies-arm64-v8a')
        self.update(dependencies_downloads=self.dependencies_downloads('1.1-20261105A'))
        self.assertEqual(self.linked('dependencies-arm64-v8a'), self.directory / 'deps-android-arm64-1.1-20261105A')
        self.assertTrue(first.is_dir(), 'what was downloaded for another version is kept')

    def test_interrupted_downloads_are_removed(self):
        interrupted = self.directory / ('deps-android-arm64-0.9-20261001A' + android_dependencies.PARTIAL_SUFFIX)
        other = self.directory / 'my-dependencies'
        interrupted.mkdir(parents=True)
        other.mkdir(parents=True)
        self.update()
        self.assertFalse(interrupted.exists())
        self.assertTrue(other.is_dir())

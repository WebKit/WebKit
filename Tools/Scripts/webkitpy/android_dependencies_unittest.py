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
import os
import shutil
import sys
import tarfile
import tempfile
import unittest
import zipfile
from unittest import mock

from webkitpy import android_dependencies
from webkitpy.android_dependencies import Download, Error, NDK_REVISION


@unittest.skipUnless(sys.platform.startswith('linux'), 'Android builds are only supported on Linux')
class AndroidDependenciesTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.temporary_directory)
        self.directory = os.path.join(self.temporary_directory, 'DependenciesAndroid')
        self.archives_directory = os.path.join(self.temporary_directory, 'archives')
        os.makedirs(self.archives_directory)
        patcher = mock.patch.object(android_dependencies, 'can_download_ndk', return_value=True)
        patcher.start()
        self.addCleanup(patcher.stop)

    def path(self, *components):
        return os.path.join(self.temporary_directory, *components)

    def create_file(self, path, contents=''):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as file:
            file.write(contents)
        return path

    def create_ndk(self, path, revision=NDK_REVISION):
        self.create_file(os.path.join(path, 'build', 'cmake', 'android.toolchain.cmake'))
        self.create_file(os.path.join(path, 'source.properties'), 'Pkg.Desc = Android NDK\nPkg.Revision = {}\n'.format(revision))
        return path

    def create_dependencies(self, path):
        os.makedirs(os.path.join(path, 'lib', 'pkgconfig'))
        return path

    def create_tarball(self, name, files, prefix=''):
        contents = tempfile.mkdtemp(dir=self.temporary_directory)
        for file, file_contents in files.items():
            self.create_file(os.path.join(contents, prefix, file), file_contents)
        tarball = os.path.join(self.archives_directory, name)
        with tarfile.open(tarball, 'w:xz') as archive:
            for entry in os.listdir(contents):
                archive.add(os.path.join(contents, entry), arcname=entry)
        return tarball

    def archive(self, path):
        return ('file://' + path, android_dependencies.sha256(path))

    def ndk_download(self):
        ndk = tempfile.mkdtemp(dir=self.temporary_directory)
        self.create_ndk(os.path.join(ndk, 'android-ndk-r29'))
        tarball = os.path.join(self.archives_directory, 'android-ndk-r29-linux.tar.xz')
        with tarfile.open(tarball, 'w:xz') as archive:
            archive.add(os.path.join(ndk, 'android-ndk-r29'), arcname='android-ndk-r29')
        return Download([self.archive(tarball)], root='android-ndk-r29')

    def dependencies_downloads(self, version='1.0-20261030A'):
        development = self.create_tarball('deps-android-arm64-{}.tar.xz'.format(version), {'lib/pkgconfig/deps.pc': ''})
        runtime = self.create_tarball('deps-android-arm64-{}-runtime.tar.xz'.format(version), {'lib/libdeps.so': ''})
        return {'arm64-v8a': Download([self.archive(development), self.archive(runtime)])}

    def update(self, environ=None, ndk_download=None, dependencies_downloads=None):
        # Keep the report of the downloads out of the test output.
        with contextlib.redirect_stdout(io.StringIO()):
            android_dependencies.update(
                self.directory, 'arm64-v8a', environ or {},
                ndk_download or self.ndk_download(), dependencies_downloads or self.dependencies_downloads())

    def linked(self, name):
        return os.path.realpath(os.path.join(self.directory, name))

    def test_download_name(self):
        download = Download([('https://example.org/deps-android-arm64-1.0-20261030A.tar.xz', ''), ('https://example.org/deps-runtime.tar.xz', '')])
        self.assertEqual(download.name, 'deps-android-arm64-1.0-20261030A')
        self.assertEqual(Download([('https://example.org/android-ndk-r29-linux.zip', '')]).name, 'android-ndk-r29-linux')

    def test_every_archive_is_unpacked_once(self):
        ndk_download = self.ndk_download()
        dependencies_downloads = self.dependencies_downloads()
        self.update(ndk_download=ndk_download, dependencies_downloads=dependencies_downloads)
        dependencies = self.linked('dependencies-arm64-v8a')
        self.assertEqual(dependencies, os.path.join(self.directory, 'deps-android-arm64-1.0-20261030A'))
        self.assertTrue(os.path.isfile(os.path.join(dependencies, 'lib', 'pkgconfig', 'deps.pc')))
        self.assertTrue(os.path.isfile(os.path.join(dependencies, 'lib', 'libdeps.so')))

        # Without the archives, a second download would fail.
        shutil.rmtree(self.archives_directory)
        self.update(ndk_download=ndk_download, dependencies_downloads=dependencies_downloads)
        self.assertEqual(self.linked('dependencies-arm64-v8a'), dependencies)

    def test_archive_with_another_checksum(self):
        tarball = self.create_tarball('other-1.0.tar.xz', {'lib/pkgconfig/other.pc': ''})
        with self.assertRaisesRegex(Error, r'^file:\S+ has the SHA-256 checksum [0-9a-f]{64} instead of 0{64}\.$'):
            self.update(dependencies_downloads={'arm64-v8a': Download([('file://' + tarball, '0' * 64)])})
        self.assertEqual(sorted(os.listdir(self.directory)), ['.lock', 'android-ndk-r29-linux'])

    @unittest.skipUnless(shutil.which('unzip'), 'unzip is not installed')
    def test_directory_of_a_zip_archive(self):
        archive = os.path.join(self.archives_directory, 'ndk-r29-linux.zip')
        with zipfile.ZipFile(archive, 'w') as zip_archive:
            zip_archive.writestr('android-ndk-r29/source.properties', 'Pkg.Revision = {}\n'.format(NDK_REVISION))
        os.makedirs(self.directory)
        with contextlib.redirect_stdout(io.StringIO()):
            path = android_dependencies.download(Download([self.archive(archive)], root='android-ndk-r29'), self.directory)
        self.assertEqual(android_dependencies.ndk_revision(path), NDK_REVISION)

    def test_given_ndk_and_dependencies_are_not_downloaded(self):
        ndk = self.create_ndk(self.path('ndk'))
        dependencies = self.create_dependencies(self.path('dependencies'))
        nothing = Download([('file:///nonexistent/nothing.tar.xz', '')])
        self.update({'WEBKIT_ANDROID_NDK': ndk, 'WEBKIT_ANDROID_DEPENDENCIES': dependencies}, nothing, {'arm64-v8a': nothing})
        self.assertEqual(self.linked('ndk'), ndk)
        self.assertEqual(self.linked('dependencies-arm64-v8a'), dependencies)
        self.assertEqual(sorted(os.listdir(self.directory)), ['.lock', 'dependencies-arm64-v8a', 'ndk'])

    def test_ndk_of_the_android_sdk(self):
        sdk = self.path('sdk')
        self.create_ndk(os.path.join(sdk, 'ndk', '27.0.12077973'), '27.0.12077973')
        self.create_ndk(os.path.join(sdk, 'ndk', '30.0.14904198'), '30.0.14904198')
        self.update({'ANDROID_HOME': sdk})
        self.assertEqual(self.linked('ndk'), os.path.join(self.directory, 'android-ndk-r29-linux'), 'the other NDKs of the Android SDK are not used')

        ndk = self.create_ndk(os.path.join(sdk, 'ndk', NDK_REVISION))
        self.update({'ANDROID_SDK_ROOT': sdk})
        self.assertEqual(self.linked('ndk'), ndk, 'the NDK r29 of the Android SDK is used, even when a newer one is installed')

        given_ndk = self.create_ndk(self.path('given-ndk'))
        self.update({'ANDROID_HOME': sdk, 'WEBKIT_ANDROID_NDK': given_ndk})
        self.assertEqual(self.linked('ndk'), given_ndk, 'WEBKIT_ANDROID_NDK is used before the NDK of the Android SDK')

    def test_another_ndk_gives_a_warning(self):
        ndk = self.create_ndk(self.path('ndk'), '27.0.12077973')
        with contextlib.redirect_stderr(io.StringIO()) as stderr:
            self.update({'WEBKIT_ANDROID_NDK': ndk})
        self.assertRegex(stderr.getvalue(), r'^Warning: WEBKIT_ANDROID_NDK is the NDK 27\.0\.12077973')
        self.assertEqual(self.linked('ndk'), ndk)

    def test_relative_paths(self):
        for variable in ('WEBKIT_ANDROID_NDK', 'WEBKIT_ANDROID_DEPENDENCIES'):
            with self.assertRaisesRegex(Error, r"^{} is set to 'relative', which is not an absolute path\.$".format(variable)):
                self.update({variable: 'relative'})

    def test_dependencies_without_pkgconfig_files(self):
        with self.assertRaisesRegex(Error, r'has no lib/pkgconfig directory\.$'):
            self.update({'WEBKIT_ANDROID_DEPENDENCIES': self.path()})

    def test_ndk_without_toolchain_file(self):
        ndk = self.create_ndk(self.path('ndk'))
        os.unlink(os.path.join(ndk, 'build', 'cmake', 'android.toolchain.cmake'))
        with self.assertRaisesRegex(Error, r"android\.toolchain\.cmake' does not exist\.$"):
            self.update({'WEBKIT_ANDROID_NDK': ndk})

    def test_ndk_is_downloaded_only_for_linux_on_x86_64(self):
        with mock.patch.object(android_dependencies, 'can_download_ndk', return_value=False):
            with self.assertRaisesRegex(Error, r'^the Android NDK can only be downloaded for Linux on x86_64\.'):
                self.update()

    def test_other_versions_are_kept(self):
        self.update(dependencies_downloads=self.dependencies_downloads('1.0-20261030A'))
        first = self.linked('dependencies-arm64-v8a')
        self.update(dependencies_downloads=self.dependencies_downloads('1.1-20261105A'))
        self.assertEqual(self.linked('dependencies-arm64-v8a'), os.path.join(self.directory, 'deps-android-arm64-1.1-20261105A'))
        self.assertTrue(os.path.isdir(first), 'what was downloaded for another version is kept')

    def test_interrupted_downloads_are_removed(self):
        interrupted = os.path.join(self.directory, 'deps-android-arm64-0.9-20261001A' + android_dependencies.PARTIAL_SUFFIX)
        other = os.path.join(self.directory, 'my-dependencies')
        os.makedirs(interrupted)
        os.makedirs(other)
        self.update()
        self.assertFalse(os.path.exists(interrupted))
        self.assertTrue(os.path.isdir(other))

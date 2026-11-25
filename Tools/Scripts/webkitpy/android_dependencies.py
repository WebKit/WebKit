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

"""
Gets the Android NDK and the dependencies that build-webkit --android builds the WPE port with.
"""

import argparse
import hashlib
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import urllib.request
from pathlib import Path

from webkitcorepy import FileLock

# The ABI of each architecture that build-webkit --android builds for.
ABIS = {
    'arm64': 'arm64-v8a',
    'x86_64': 'x86_64',
}


class Download:
    def __init__(self, url, checksum):
        self.url = url
        # The SHA-256 checksum of the archive.
        self.checksum = checksum

    @property
    def filename(self):
        return self.url.rsplit('/', 1)[-1]

    @property
    def name(self):
        for extension in ('.zip', '.tar.xz'):
            if self.filename.endswith(extension):
                return self.filename.removesuffix(extension)
        return self.filename

    def fetch(self, path):
        print(f'Downloading {self.url}', flush=True)
        try:
            with urllib.request.urlopen(self.url) as response, path.open('wb') as file:
                shutil.copyfileobj(response, file, 1024 * 1024)
        except OSError as error:
            raise SystemExit(f'Could not download {self.url}: {error}')
        checksum = sha256(path)
        if checksum != self.checksum:
            raise SystemExit(f'{self.url} has the SHA-256 checksum {checksum} instead of {self.checksum}.')


# Same NDK version used to build the dependencies.
NDK_REVISION = '29.0.14206865'
NDK_DOWNLOADS = [
    Download('https://dl.google.com/android/repository/android-ndk-r29-linux.zip', '4abbbcdc842f3d4879206e9695d52709603e52dd68d3c1fff04b3b5e7a308ecf'),
]
# The directory of the NDK archive that has the NDK.
NDK_ROOT = 'android-ndk-r29'

# The wpewebkit-deps packages of https://github.com/Igalia/wpe-android-cerbero, which are the
# dependencies of WPE WebKit without WPE WebKit itself. Their names have the date of their upload,
# since each upload changes what the names without a date point to. See
# https://github.com/Igalia/wpe-android-cerbero/blob/main/docs/raise-webkit-dependencies.md
# for how to update them.
DEPENDENCIES_URL = 'https://wpewebkit.org/android/bootstrap'
DEPENDENCIES_VERSION = '2.55.0.8'
DEPENDENCIES_UPLOAD = '20261103A'


def dependencies_url(architecture, package=None):
    package = f'-{package}' if package else ''
    return f'{DEPENDENCIES_URL}/{DEPENDENCIES_VERSION}/wpewebkit-deps-android-{architecture}-{DEPENDENCIES_VERSION}{package}-{DEPENDENCIES_UPLOAD}.tar.xz'


DEPENDENCIES_DOWNLOADS = {
    'arm64-v8a': [
        Download(dependencies_url('arm64'), 'c94b6fa937574ad7d2244670e4fabecbe5f4c25d983927afdcaee97debc8f1d3'),
        Download(dependencies_url('arm64', 'runtime'), '4e3b1aeb41f12484f64f6b31c873e4c11263f2006638cd334b762aedbe5a92de'),
    ],
    'x86_64': [
        Download(dependencies_url('x86_64'), '217490afa7ece91affa11b77cf0c67df2f68b2f88c839e9570384d51676116f0'),
        Download(dependencies_url('x86_64', 'runtime'), '3c5f9f82f64d32f1d144bea664e9b91583cd8d8aba35e28f9ad7242304455230'),
    ],
}

PARTIAL_SUFFIX = '.partial'

# A build for another architecture may be downloading the NDK, which takes a while.
LOCK_TIMEOUT = 60 * 60


def absolute_path_from_environment(environ, variable):
    path = environ.get(variable)
    if not path:
        return None
    # build-webkit runs from the source directory, and CMake from the build directory, so a relative
    # path would not point to the same place for both.
    if not os.path.isabs(path):
        raise SystemExit(f"{variable} is set to '{path}', which is not an absolute path.")
    return Path(os.path.normpath(path))


def ndk_revision(path):
    try:
        with (path / 'source.properties').open() as properties:
            for line in properties:
                key, _, value = line.partition('=')
                if key.strip() == 'Pkg.Revision':
                    return value.strip()
    except OSError:
        pass
    return None


def installed_ndk(environ):
    path = absolute_path_from_environment(environ, 'WEBKIT_ANDROID_NDK')
    if path:
        revision = ndk_revision(path) or 'unknown'
        if revision != NDK_REVISION:
            print(f'Warning: WEBKIT_ANDROID_NDK is the NDK {revision}, but the dependencies are built with the NDK {NDK_REVISION}.', file=sys.stderr)
        return path

    # The Android SDK installs each NDK in a directory named after its revision.
    for variable in ('ANDROID_HOME', 'ANDROID_SDK_ROOT'):
        if environ.get(variable):
            path = Path(environ[variable]) / 'ndk' / NDK_REVISION
            if path.is_dir():
                return path
    return None


def installed_dependencies(environ):
    path = absolute_path_from_environment(environ, 'WEBKIT_ANDROID_DEPENDENCIES')
    if path and not (path / 'lib' / 'pkgconfig').is_dir():
        raise SystemExit(f"WEBKIT_ANDROID_DEPENDENCIES is set to '{path}', which has no lib/pkgconfig directory.")
    return path


def can_download_ndk():
    # The NDK that is downloaded is the one for Linux on x86_64.
    return platform.system() == 'Linux' and platform.machine() == 'x86_64'


def sha256(path):
    checksum = hashlib.sha256()
    with path.open('rb') as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b''):
            checksum.update(chunk)
    return checksum.hexdigest()


def unpack(path, directory):
    if path.suffix == '.zip':
        # zipfile would lose the symbolic links of the NDK, and the executable bits of its tools.
        try:
            subprocess.run(['unzip', '-q', str(path), '-d', str(directory)], check=True)
        except (OSError, subprocess.CalledProcessError) as error:
            raise SystemExit(f'Could not unpack {path}: {error}')
        return
    with tarfile.open(path) as archive:
        # Python 3.14 uses this filter by default, and older versions warn without one.
        if hasattr(tarfile, 'data_filter'):
            archive.extractall(directory, filter='data')
        else:
            archive.extractall(directory)


def download(downloads, directory, root=None):
    # Another version is unpacked in another directory, and the ones already unpacked are kept.
    path = directory / downloads[0].name
    if path.is_dir():
        return path

    partial = directory / (path.name + PARTIAL_SUFFIX)
    contents = partial / 'contents'
    contents.mkdir(parents=True)
    try:
        for archive in downloads:
            file = partial / archive.filename
            archive.fetch(file)
            unpack(file, contents)
            file.unlink()
        (contents / root if root else contents).rename(path)
    finally:
        shutil.rmtree(partial, ignore_errors=True)
    return path


def remove_partial_downloads(directory):
    for path in directory.glob('*' + PARTIAL_SUFFIX):
        shutil.rmtree(path, ignore_errors=True)


def link(path, target):
    if path.is_symlink() and path.readlink() == target:
        return
    temporary = path.with_name(path.name + '.new')
    temporary.unlink(missing_ok=True)
    temporary.symlink_to(target)
    temporary.replace(path)


def update(directory, abi, environ=os.environ, ndk_downloads=NDK_DOWNLOADS, dependencies_downloads=DEPENDENCIES_DOWNLOADS):
    directory = Path(directory).absolute()
    ndk = installed_ndk(environ)
    if not ndk and not can_download_ndk():
        raise SystemExit(f'The Android NDK can only be downloaded for Linux on x86_64. Set WEBKIT_ANDROID_NDK to the NDK {NDK_REVISION}.')
    dependencies = installed_dependencies(environ)

    directory.mkdir(parents=True, exist_ok=True)
    # Builds for other architectures share the NDK, and may run at the same time.
    with FileLock(str(directory / '.lock'), timeout=LOCK_TIMEOUT):
        remove_partial_downloads(directory)
        ndk = ndk or download(ndk_downloads, directory, root=NDK_ROOT)
        dependencies = dependencies or download(dependencies_downloads[abi], directory)

        toolchain_file = ndk / 'build' / 'cmake' / 'android.toolchain.cmake'
        if not toolchain_file.is_file():
            raise SystemExit(f"'{toolchain_file}' does not exist.")

        link(directory / 'ndk', ndk)
        link(directory / f'dependencies-{abi}', dependencies)


def default_directory():
    source_directory = Path(__file__).absolute().parents[3]
    build_directory = os.environ.get('WEBKIT_OUTPUTDIR') or source_directory / 'WebKitBuild'
    return Path(build_directory) / 'DependenciesAndroid'


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=f'Gets the Android NDK {NDK_REVISION} and the dependencies that build-webkit --android builds the WPE port with.',
        epilog=f'The NDK is the one in WEBKIT_ANDROID_NDK, or else the ndk/{NDK_REVISION} directory of the Android SDK in ANDROID_HOME '
               'or ANDROID_SDK_ROOT, and the dependencies are the ones in WEBKIT_ANDROID_DEPENDENCIES; both variables take '
               'absolute paths. What is not given is downloaded once and kept: remove the directory to free the space.',
    )
    parser.add_argument('--architecture', choices=sorted(ABIS), default='arm64', help='the architecture of the build (default: %(default)s)')
    parser.add_argument('--directory', type=Path, default=default_directory(), help='where to download, which build-webkit gives (default: %(default)s)')
    args = parser.parse_args(argv)

    update(args.directory, ABIS[args.architecture])
    return 0

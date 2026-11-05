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

They are the ones given in the environment, or else downloaded, and the links that build-webkit
looks for in the directory of the downloads point to them.
"""

import argparse
import hashlib
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import time
import urllib.request

from webkitcorepy import FileLock

# The ABI of each architecture that build-webkit --android builds for.
ABIS = {
    'arm64': 'arm64-v8a',
    'x86_64': 'x86_64',
}


class Download(object):
    def __init__(self, archives, root=None):
        # The URL and the SHA-256 checksum of each archive, which are unpacked together.
        self.archives = archives
        # The directory of the archives that has what is downloaded, when that is not all of them.
        self.root = root

    @property
    def name(self):
        # Another version is unpacked in another directory, and the ones already unpacked are kept.
        name = os.path.basename(self.archives[0][0])
        for extension in ('.zip', '.tar.xz'):
            if name.endswith(extension):
                return name[:-len(extension)]
        return name


# Same NDK version used to build the dependencies.
NDK_REVISION = '29.0.14206865'
NDK_DOWNLOAD = Download(
    [('https://dl.google.com/android/repository/android-ndk-r29-linux.zip', '4abbbcdc842f3d4879206e9695d52709603e52dd68d3c1fff04b3b5e7a308ecf')],
    root='android-ndk-r29',
)

# The wpewebkit-deps packages of https://github.com/Igalia/wpe-android-cerbero, which are the
# dependencies of WPE WebKit without WPE WebKit itself. Their names have the date of their upload,
# since each upload changes what the names without a date point to. See
# https://github.com/Igalia/wpe-android-cerbero/blob/main/docs/raise-webkit-dependencies.md
# for how to update them.
DEPENDENCIES_DOWNLOADS = {
    'arm64-v8a': Download([
        ('https://wpewebkit.org/android/bootstrap/2.55.0.8/wpewebkit-deps-android-arm64-2.55.0.8-20261103A.tar.xz', 'c94b6fa937574ad7d2244670e4fabecbe5f4c25d983927afdcaee97debc8f1d3'),
        ('https://wpewebkit.org/android/bootstrap/2.55.0.8/wpewebkit-deps-android-arm64-2.55.0.8-runtime-20261103A.tar.xz', '4e3b1aeb41f12484f64f6b31c873e4c11263f2006638cd334b762aedbe5a92de'),
    ]),
    'x86_64': Download([
        ('https://wpewebkit.org/android/bootstrap/2.55.0.8/wpewebkit-deps-android-x86_64-2.55.0.8-20261103A.tar.xz', '217490afa7ece91affa11b77cf0c67df2f68b2f88c839e9570384d51676116f0'),
        ('https://wpewebkit.org/android/bootstrap/2.55.0.8/wpewebkit-deps-android-x86_64-2.55.0.8-runtime-20261103A.tar.xz', '3c5f9f82f64d32f1d144bea664e9b91583cd8d8aba35e28f9ad7242304455230'),
    ]),
}

PARTIAL_SUFFIX = '.partial'

# A build for another architecture may be downloading the NDK, which takes a while.
LOCK_TIMEOUT = 60 * 60


class Error(Exception):
    pass


def default_directory():
    source_directory = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    build_directory = os.environ.get('WEBKIT_OUTPUTDIR') or os.path.join(source_directory, 'WebKitBuild')
    return os.path.join(build_directory, 'DependenciesAndroid')


def absolute_path_from_environment(environ, variable):
    path = environ.get(variable)
    if not path:
        return None
    # build-webkit runs from the source directory, and CMake from the build directory, so a relative
    # path would not point to the same place for both.
    if not os.path.isabs(path):
        raise Error("{} is set to '{}', which is not an absolute path.".format(variable, path))
    return os.path.normpath(path)


def ndk_revision(path):
    try:
        with open(os.path.join(path, 'source.properties')) as properties:
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
            print('Warning: WEBKIT_ANDROID_NDK is the NDK {}, but the dependencies are built with the NDK {}.'.format(revision, NDK_REVISION), file=sys.stderr)
        return path

    # The Android SDK installs each NDK in a directory named after its revision.
    for variable in ('ANDROID_HOME', 'ANDROID_SDK_ROOT'):
        if environ.get(variable):
            path = os.path.join(environ[variable], 'ndk', NDK_REVISION)
            if os.path.isdir(path):
                return path
    return None


def installed_dependencies(environ):
    path = absolute_path_from_environment(environ, 'WEBKIT_ANDROID_DEPENDENCIES')
    if path and not os.path.isdir(os.path.join(path, 'lib', 'pkgconfig')):
        raise Error("WEBKIT_ANDROID_DEPENDENCIES is set to '{}', which has no lib/pkgconfig directory.".format(path))
    return path


def can_download_ndk():
    # The NDK that is downloaded is the one for Linux on x86_64.
    return platform.system() == 'Linux' and platform.machine() == 'x86_64'


def fetch(url, path):
    attempts = 3
    for attempt in range(1, attempts + 1):
        try:
            with urllib.request.urlopen(url) as response, open(path, 'wb') as file:
                shutil.copyfileobj(response, file, 1024 * 1024)
            return
        except OSError as error:
            if attempt == attempts:
                raise Error('could not download {}: {}'.format(url, error))
            time.sleep(5)


def sha256(path):
    checksum = hashlib.sha256()
    with open(path, 'rb') as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b''):
            checksum.update(chunk)
    return checksum.hexdigest()


def unpack(path, directory):
    if path.endswith('.zip'):
        # zipfile would lose the symbolic links of the NDK, and the executable bits of its tools.
        if subprocess.run(['unzip', '-q', path, '-d', directory]).returncode:
            raise Error('could not unpack {}.'.format(path))
        return
    with tarfile.open(path) as archive:
        # Python 3.14 uses this filter by default, and older versions warn without one.
        if hasattr(tarfile, 'data_filter'):
            archive.extractall(directory, filter='data')
        else:
            archive.extractall(directory)


def download(what, directory):
    path = os.path.join(directory, what.name)
    checksums = ''.join('{}  {}\n'.format(checksum, os.path.basename(url)) for url, checksum in what.archives)
    try:
        with open(os.path.join(path, '.sha256sums')) as file:
            if file.read() == checksums:
                return path
    except OSError:
        pass

    partial = path + PARTIAL_SUFFIX
    contents = os.path.join(partial, 'contents')
    os.makedirs(contents)
    try:
        for url, checksum in what.archives:
            file = os.path.join(partial, os.path.basename(url))
            print('Downloading {}'.format(url), flush=True)
            fetch(url, file)
            file_checksum = sha256(file)
            if file_checksum != checksum:
                raise Error('{} has the SHA-256 checksum {} instead of {}.'.format(url, file_checksum, checksum))
            unpack(file, contents)
            os.unlink(file)

        root = os.path.join(contents, what.root) if what.root else contents
        with open(os.path.join(root, '.sha256sums'), 'w') as file:
            file.write(checksums)
        shutil.rmtree(path, ignore_errors=True)
        os.rename(root, path)
    finally:
        shutil.rmtree(partial, ignore_errors=True)
    return path


def remove_partial_downloads(directory):
    for name in os.listdir(directory):
        if name.endswith(PARTIAL_SUFFIX):
            shutil.rmtree(os.path.join(directory, name), ignore_errors=True)


def link(path, target):
    if os.path.islink(path) and os.readlink(path) == target:
        return
    temporary = path + '.new'
    if os.path.lexists(temporary):
        os.unlink(temporary)
    os.symlink(target, temporary)
    os.replace(temporary, path)


def update(directory, abi, environ=os.environ, ndk_download=NDK_DOWNLOAD, dependencies_downloads=DEPENDENCIES_DOWNLOADS):
    directory = os.path.abspath(directory)
    ndk = installed_ndk(environ)
    if not ndk and not can_download_ndk():
        raise Error('the Android NDK can only be downloaded for Linux on x86_64. Set WEBKIT_ANDROID_NDK to the NDK {}.'.format(NDK_REVISION))
    dependencies = installed_dependencies(environ)

    os.makedirs(directory, exist_ok=True)
    # Builds for other architectures share the NDK, and may run at the same time.
    with FileLock(os.path.join(directory, '.lock'), timeout=LOCK_TIMEOUT):
        remove_partial_downloads(directory)
        ndk = ndk or download(ndk_download, directory)
        dependencies = dependencies or download(dependencies_downloads[abi], directory)

        toolchain_file = os.path.join(ndk, 'build', 'cmake', 'android.toolchain.cmake')
        if not os.path.isfile(toolchain_file):
            raise Error("'{}' does not exist.".format(toolchain_file))

        link(os.path.join(directory, 'ndk'), ndk)
        link(os.path.join(directory, 'dependencies-' + abi), dependencies)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description='Gets the Android NDK {} and the dependencies that build-webkit --android builds the WPE port with.'.format(NDK_REVISION),
        epilog='The NDK is the one in WEBKIT_ANDROID_NDK, or else the ndk/{} directory of the Android SDK in ANDROID_HOME '
               'or ANDROID_SDK_ROOT, and the dependencies are the ones in WEBKIT_ANDROID_DEPENDENCIES; both variables take '
               'absolute paths. What is not given is downloaded once and kept: remove the directory to free the space.'.format(NDK_REVISION),
    )
    parser.add_argument('--architecture', choices=sorted(ABIS), default='arm64', help='the architecture of the build (default: %(default)s)')
    parser.add_argument('--directory', default=default_directory(), help='where to download, which build-webkit gives (default: %(default)s)')
    args = parser.parse_args(argv)

    try:
        update(args.directory, ABIS[args.architecture])
    except Error as error:
        print('Error: {}'.format(error), file=sys.stderr)
        return 1
    return 0

# Copyright (C) 2020 Apple Inc. All rights reserved.
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

import json
import logging
import math
import os
import platform
import re
import subprocess
import shutil
import sys
import tarfile
import tempfile
import zipfile

from logging import NullHandler
from webkitcorepy import log
from webkitcorepy.version import Version
from xml.dom import minidom

if sys.version_info > (3, 0):
    from urllib.request import urlopen
    from urllib.error import URLError
else:
    from urllib2 import urlopen, URLError


class Package(object):
    class Archive(object):
        def __init__(self, name, link, version, extension=None):
            self.name = name
            self.link = link
            self.version = version
            self.extension = extension or 'tar.gz'

        def __repr__(self):
            return '{}-{}.{}.{}'.format(self.name, self.version.major, self.version.minor, self.version.tiny)

        @property
        def path(self):
            if not AutoInstall.directory:
                raise ValueError('No AutoInstall directory, archive cannot resolve local path')
            return '{}/{}-{}.{}'.format(AutoInstall.directory, self.name, self.version, self.extension)

        def download(self):
            response = AutoInstall._request(self.link)
            try:
                if response.code != 200:
                    raise IOError('Failed to retrieve Python module with response code {}'.format(response.code))
                with open(self.path, 'wb') as file:
                    while True:
                        data = response.read(2 ** 13)
                        if not data:
                            break
                        file.write(data)
            finally:
                response.close()

        def unpack(self, target):
            if not os.path.isfile(self.path):
                raise IOError('Failed to find archive at {}'.format(self.path))
            shutil.rmtree(target, ignore_errors=True)

            if self.extension == 'tar.gz':
                file = tarfile.open(self.path)
                try:
                    file.extractall(target)
                finally:
                    file.close()
            elif self.extension in ['whl', 'zip']:
                with zipfile.ZipFile(self.path, 'r') as file:
                    file.extractall(target)
            else:
                raise OSError('{} has an  unrecognized package format'.format(self.path))

    def __init__(self, name, version=None, pypi_name=None, slow_install=False, wheel=False, aliases=None):
        self.name = name
        self.version = version
        self._archives = []
        self.pypi_name = pypi_name or self.name
        self.slow_install = slow_install
        self.wheel = wheel
        self.aliases = aliases or []

    @property
    def location(self):
        if not AutoInstall.directory:
            raise ValueError('No AutoInstall directory, Package cannot resolve location')
        return os.path.join(AutoInstall.directory, self.name)

    def do_post_install(self, archive_path):
        pass

    def archives(self):
        if self._archives:
            return self._archives

        path = 'simple/{}/'.format(self.pypi_name)
        response = AutoInstall._request('https://{}/{}'.format(AutoInstall.index, path))
        try:
            if response.code != 200:
                raise ValueError('The package {} was not found on {}'.format(self.pypi_name, AutoInstall.index))

            page = minidom.parseString(response.read())
            cached_tags = None

            for element in reversed(page.getElementsByTagName("a")):
                if not len(element.childNodes):
                    continue
                if element.childNodes[0].nodeType != minidom.Node.TEXT_NODE:
                    continue

                attributes = {}
                for index in range(element.attributes.length):
                    attributes[element.attributes.item(index).name] = element.attributes.item(index).value
                if not attributes.get('href', None):
                    continue

                if self.wheel:
                    match = re.search(r'.+-([^-]+-[^-]+-[^-]+).whl', element.childNodes[0].data)
                    if not match:
                        continue

                    from packaging import tags

                    if not cached_tags:
                        cached_tags = set(AutoInstall.tags())

                    if all([tag not in cached_tags for tag in tags.parse_tag(match.group(1))]):
                        continue

                    extension = 'whl'

                else:
                    if element.childNodes[0].data.endswith('.tar.gz'):
                        extension = 'tar.gz'
                    elif element.childNodes[0].data.endswith('.zip'):
                        extension = 'zip'
                    else:
                        continue

                requires = attributes.get('data-requires-python')
                if requires and not AutoInstall.version.matches(requires):
                    continue

                version_candidate = re.search(r'\d+\.\d+(\.\d+)?', element.childNodes[0].data)
                if not version_candidate:
                    continue
                version = Version(*version_candidate.group().split('.'))
                if self.version and version not in self.version:
                    continue

                link = attributes['href'].split('#')[0]
                if '://' not in link:
                    depth = 0
                    while link.startswith('../'):
                        depth += 1
                        link = link[3:]
                    link = 'https://{}/{}{}'.format(AutoInstall.index, '/'.join(path.split('/')[depth:]), link)

                self._archives.append(self.Archive(
                    name=self.pypi_name,
                    link=link,
                    version=version,
                    extension=extension,
                ))

            self._archives = sorted(self._archives, key=lambda archive: archive.version)
            return self._archives
        finally:
            response.close()

    def is_cached(self):
        manifest = AutoInstall.manifest.get(self.name)
        if not manifest:
            return False
        if AutoInstall.overwrite_foreign_packages and manifest.get('index') != AutoInstall.index:
            return False
        if not manifest.get('version'):
            return False
        return not self.version or Version(*manifest.get('version').split('.')) in self.version

    def install(self):
        AutoInstall.register(self)
        if self.is_cached():
            return

        # Make sure that setuptools are installed, since setup.py relies on it
        if self.name != 'setuptools':
            AutoInstall.install('setuptools')

        if not self.archives():
            raise ValueError('No archives for {}-{} found'.format(self.pypi_name, self.version))
        archive = self.archives()[-1]

        try:
            install_location = os.path.dirname(self.location)
            shutil.rmtree(self.location, ignore_errors=True)

            AutoInstall.log('Downloading {}...'.format(archive))
            archive.download()

            temp_location = os.path.join(tempfile.gettempdir(), self.name)
            archive.unpack(temp_location)

            for candidate in [
                os.path.join(temp_location, str(archive)),
                os.path.join(temp_location, '{}-{}.{}'.format(archive.name, archive.version.major, archive.version.minor)),
                os.path.join(temp_location, '{}-{}.{}.{}'.format(archive.name.replace('-', '_'), archive.version.major, archive.version.minor, archive.version.tiny)),
                os.path.join(temp_location, '{}-{}'.format(archive.name.capitalize(), archive.version)),
            ]:
                if not os.path.exists(os.path.join(candidate, 'setup.py')):
                    continue

                AutoInstall.log('Installing {}...'.format(archive))

                if self.slow_install:
                    AutoInstall.log('{} is known to be slow to install'.format(archive))

                with open(os.devnull, 'w') as devnull:
                    subprocess.check_call(
                        [
                            sys.executable,
                            os.path.join(candidate, 'setup.py'),
                            'install',
                            '--home={}'.format(install_location),
                            '--root=/',
                            '--single-version-externally-managed',
                            '--install-lib={}'.format(install_location),
                            '--install-scripts={}'.format(os.path.join(install_location, 'bin')),
                            '--install-data={}'.format(os.path.join(install_location, 'data')),
                            '--install-headers={}'.format(os.path.join(install_location, 'headers')),
                            # Do not automatically install package dependencies, force scripts to be explicit
                            # Even without this flag, setup.py is not consistent about installing dependencies.
                            '--old-and-unmanageable',
                        ],
                        cwd=candidate,
                        env=dict(
                            PATH=os.environ.get('PATH', ''),
                            PATHEXT=os.environ.get('PATHEXT', ''),
                            PYTHONPATH=install_location,
                            SYSTEMROOT=os.environ.get('SYSTEMROOT', ''),
                        ),
                        stdout=devnull,
                        stderr=devnull,
                    )

                break
            else:
                # We might not need setup.py at all, check if we have dist-info and the library in the temporary location
                to_be_moved = os.listdir(temp_location)
                if self.name not in to_be_moved and any(element.endswith('.dist-info') for element in to_be_moved):
                    raise OSError('Cannot install {}, could not find setup.py'.format(self.name))
                for directory in to_be_moved:
                    shutil.rmtree(os.path.join(install_location, directory), ignore_errors=True)
                    shutil.move(os.path.join(temp_location, directory), install_location)

            self.do_post_install(temp_location)

            os.remove(archive.path)
            shutil.rmtree(temp_location, ignore_errors=True)

            AutoInstall.userspace_should_own(install_location)

            AutoInstall.manifest[self.name] = {
                'index': AutoInstall.index,
                'version': str(archive.version),
            }

            manifest = os.path.join(AutoInstall.directory, 'manifest.json')
            with open(manifest, 'w') as file:
                json.dump(AutoInstall.manifest, file, indent=4)
            AutoInstall.userspace_should_own(manifest)

            AutoInstall.log('Installed {}!'.format(archive))
        except Exception:
            AutoInstall.log('Failed to install {}!'.format(archive), level=logging.CRITICAL)
            raise


class AutoInstall(object):
    DISABLE_ENV_VAR = 'DISABLE_WEBKITCOREPY_AUTOINSTALLER'

    directory = None
    index = 'pypi.org'
    timeout = 30
    version = Version(sys.version_info[0], sys.version_info[1], sys.version_info[2])
    packages = {}
    manifest = {}

    # When sharing an install location, projects may wish to overwrite packages on disk
    # originating from a different index.
    overwrite_foreign_packages = False

    @classmethod
    def _request(cls, url):
        # Rely on our own certificates for PyPi, since we use PyPi to standardize root certificates
        if url.startswith('https://pypi.org') or url.startswith('https://files.pythonhosted.org'):
            return urlopen(
                url,
                timeout=cls.timeout,
                cafile=os.path.join(os.path.dirname(__file__), 'cacert.pem'),
            )
        return urlopen(url, timeout=cls.timeout)

    @classmethod
    def enabled(cls):
        if os.environ.get(cls.DISABLE_ENV_VAR) not in ['0', 'FALSE', 'False', 'false', 'NO', 'No', 'no', None]:
            return False
        return True if cls.directory else None

    @classmethod
    def userspace_should_own(cls, path):
        # Windows doesn't have sudo
        if not hasattr(os, "geteuid"):
            return

        # If we aren't root, the default behavior is correct
        if os.geteuid() != 0:
            return

        # If running as sudo, we really want the caller of sudo to own the autoinstall directory
        uid = int(os.environ.get('SUDO_UID', -1))
        gid = int(os.environ.get('SUDO_GID', -1))

        os.chown(path, uid, gid)
        if not os.path.isdir(path):
            return

        for root, directories, files in os.walk(path):
            for directory in directories:
                os.chown(os.path.join(root, directory), uid, gid)
            for file in files:
                os.chown(os.path.join(root, file), uid, gid)

    @classmethod
    def set_directory(cls, directory):
        if not directory or not isinstance(directory, str):
            raise ValueError('{} is an invalid autoinstall directory'.format(directory))

        if cls.enabled() is False:
            AutoInstall.log('Request to set autoinstall directory to {}'.format(directory))
            AutoInstall.log('Environment variable {}={} overriding request'.format(
                cls.DISABLE_ENV_VAR,
                os.environ.get(cls.DISABLE_ENV_VAR),
            ))
            return

        directory = os.path.abspath(directory)
        if not os.path.isdir(directory):
            creation_root = directory
            while not os.path.isdir(os.path.dirname(creation_root)):
                creation_root = os.path.dirname(creation_root)

            if os.path.exists(directory):
                raise ValueError('{} is not a directory and cannot be used as the autoinstall location')
            os.makedirs(directory)

            cls.userspace_should_own(creation_root)

        try:
            with open(os.path.join(directory, 'manifest.json'), 'r') as file:
                cls.manifest = json.load(file)
        except (IOError, OSError, ValueError):
            pass

        sys.path.insert(0, directory)
        cls.directory = directory

    @classmethod
    def set_index(cls, index, check=True, fatal=False):
        if not check:
            cls.index = index
            return cls.index

        def error(message):
            if fatal:
                raise ValueError(message)

            sys.stderr.write('{}\n'.format(message))
            sys.stderr.write('Falling back to current index, {}\n\n'.format(cls.index))

        response = None
        try:
            response = AutoInstall._request('https://{}/simple/pip/'.format(index))
            if response.code != 200:
                error('Failed to set AutoInstall index to {}, received {} response when searching for simple/pip'.format(index, response.code))
            else:
                cls.index = index
        except URLError:
            error('Failed to set AutoInstall index to {}, no response from the server'.format(index))
        finally:
            if response:
                response.close()

        return cls.index


    @classmethod
    def set_timeout(cls, timeout):
        if timeout is not None and timeout <= 0:
            raise ValueError('{} is an invalid timeout value'.format(timeout))
        cls.timeout = math.ceil(timeout)

    @classmethod
    def register(cls, package, local=False):
        if isinstance(package, str):
            if cls.packages.get(package):
                return cls.packages[package]
            else:
                package = Package(package)
        elif isinstance(package, Package):
            if cls.packages.get(package.name):
                if cls.packages.get(package.name).version != package.version:
                    raise ValueError('Registered version of {} uses {}, but requested version uses {}'.format(package.name, cls.packages.get(package.name).version, package.version))
                return cls.packages.get(package.name)
        else:
            raise ValueError('Expected package to be str or Package, not {}'.format(type(package)))

        # If inside the WebKit checkout, a local library is likely checked in at Tools/Scripts/libraries.
        # When we detect such a library, we should not register it to be auto-installed
        if local and package.name != 'autoinstalled':
            libraries = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
            checkout_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(libraries))))
            for candidate in [
                os.path.join(libraries, package.pypi_name),
                os.path.join(checkout_root, 'Internal', 'Tools', 'Scripts', 'libraries', package.pypi_name),
            ]:
                if candidate in sys.path:
                    return package
                if not os.path.isdir(os.path.join(candidate, package.name)):
                    continue
                sys.path.insert(0, candidate)
                return package

        for alias in package.aliases:
            cls.packages[alias] = package
        cls.packages[package.name] = package
        return package

    @classmethod
    def install(cls, package):
        to_install = cls.register(package)
        return to_install.install()

    @classmethod
    def install_everything(cls):
        for package in cls.packages.values():
            package.install()
        return None

    @classmethod
    def find_module(cls, fullname, path=None):
        if not cls.enabled() or path is not None:
            return

        name = fullname.split('.')[0]
        if cls.packages.get(name):
            cls.install(name)

    @classmethod
    def tags(cls):
        from packaging import tags

        for tag in tags.sys_tags():
            yield tag

        # FIXME: Work around for https://github.com/pypa/packaging/pull/319 and Big Sur
        if sys.platform == 'darwin' and Version.from_string(platform.mac_ver()[0]) > Version(10):
            for override in tags.mac_platforms(version=(10, 16)):
                for tag in tags.sys_tags():
                    if not tag.platform:
                        pass
                    yield tags.Tag(tag.interpreter, tag.abi, override)

    @classmethod
    def log(cls, message, level=logging.WARNING):
        if not log.handlers or all([isinstance(handle, NullHandler) for handle in log.handlers]):
            sys.stderr.write(message + '\n')
        else:
            log.log(level, message)


sys.meta_path.insert(0, AutoInstall)

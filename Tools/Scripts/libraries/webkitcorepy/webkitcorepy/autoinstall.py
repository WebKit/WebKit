# Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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
import shutil
import ssl
import subprocess
import sys
import tarfile
import tempfile
import time
import zipfile

from collections import defaultdict
from contextlib import contextmanager
from logging import NullHandler
from webkitcorepy import log
from webkitcorepy.version import Version
from webkitcorepy.file_lock import FileLock

from html.parser import HTMLParser
from importlib import machinery as importmachinery
from urllib.request import urlopen
from urllib.error import URLError


class SimplyPypiIndexPageParser(HTMLParser):
    def __init__(self):
        HTMLParser.__init__(self)
        self.packages = []
        self.current_package = None

    def handle_starttag(self, tag, attrs):
        if tag == "a":
            attrs_dict = dict(attrs)
            if "href" not in attrs_dict:
                return
            self.current_package = attrs_dict

    def handle_data(self, data):
        if self.current_package is not None:
            self.current_package["name"] = data

    def handle_endtag(self, tag):
        if tag == "a" and self.current_package is not None:
            self.packages.append(self.current_package)
            self.current_package = None

    @staticmethod
    def parse(html_text):
        parser = SimplyPypiIndexPageParser()
        parser.feed(html_text)
        return parser.packages


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
            AutoInstall._verify_index()
            count = 0
            while count <= (AutoInstall.times_to_retry or 0):
                response = None
                try:
                    response = AutoInstall._request(self.link)
                    if not response or response.code != 200:
                        raise IOError('Failed to retrieve Python module with response code {}'.format(response.code))
                    with open(self.path, 'wb') as file:
                        while True:
                            data = response.read(2 ** 13)
                            if not data:
                                break
                            file.write(data)
                    return
                except (IOError, URLError) as e:
                    if count == (AutoInstall.times_to_retry or 0):
                        raise
                    else:
                        AutoInstall.log(str(e))
                        AutoInstall.log('Failed to download {}, retrying'.format(self.name))
                finally:
                    if response:
                        response.close()
                    count += 1
            # This is unreachable because the raise in the above loop should always return or raise.
            raise RuntimeError("unreachable")

        def unpack(self, target):
            if not os.path.isfile(self.path):
                raise IOError('Failed to find archive at {}'.format(self.path))
            shutil.rmtree(target, ignore_errors=True)

            if self.extension == 'tar.gz':
                file = tarfile.open(self.path)
                # Prevent write-protected files which can't be overwritten by manually setting permissions
                for tarred in file:
                    tarred.mode = 0o777 if tarred.isdir() else (0o644 | (tarred.mode & 0o111))
                try:
                    file.extractall(target)
                finally:
                    file.close()
            elif self.extension in ['whl', 'zip']:
                with zipfile.ZipFile(self.path, 'r') as file:
                    for zip_info in file.infolist():
                        file_path = file.extract(zip_info, target)
                        mode = (zip_info.external_attr >> 16) & 0x1FF
                        mode = 0o777 if getattr(zip_info, 'is_dir', lambda: False)() else (0o644 | (mode & 0o111))
                        os.chmod(file_path, mode)
            else:
                raise OSError('{} has an unrecognized package format'.format(self.path))

    def __init__(self, import_name, version=None, pypi_name=None, slow_install=False, wheel=None, aliases=None, implicit_deps=None):
        self.name = import_name
        self.version = version
        self._archives = []
        self.pypi_name = pypi_name or self.name
        self.slow_install = slow_install
        self.wheel = wheel
        self.aliases = aliases or []
        self.implicit_deps = implicit_deps or []

    def __repr__(self):
        return ("Package("
                "import_name={self.name!r}, "
                "version={self.version!r}, "
                "pypi_name={self.pypi_name!r}, "
                "slow_install={self.slow_install!r}, "
                "wheel={self.wheel!r}, "
                "aliases={self.aliases!r}, "
                "implicit_deps={self.implicit_deps!r}"
                ")"
                ).format(self=self)

    @property
    def location(self):
        if not AutoInstall.directory:
            raise ValueError('No AutoInstall directory, Package cannot resolve location')
        return os.path.join(AutoInstall.directory, *self.name.split('.'))

    def do_post_install(self, archive_path):
        pass

    def archives(self):
        if self._archives:
            return self._archives

        AutoInstall._verify_index()
        path = 'simple/{}/'.format(self.pypi_name)
        count = 0
        while count <= (AutoInstall.times_to_retry or 0):
            response = None
            try:
                response = AutoInstall._request('https://{}/{}'.format(AutoInstall.index, path))
                if response.code != 200:
                    raise ValueError('The package {} was not found on {}'.format(self.pypi_name, AutoInstall.index))

                packages = SimplyPypiIndexPageParser.parse(response.read().decode("UTF-8"))
                cached_tags = None

                for package in reversed(packages):
                    if self.wheel or self.wheel is None and package['name'].endswith('.whl'):
                        match = re.search(r'.+-([^-]+-[^-]+-[^-]+).whl', package['name'])
                        if not match:
                            continue

                        # Temporarily disable AutoInstall so we don't try to AutoInstall
                        # packaging via the below import while installing packaging.
                        with AutoInstall.temporarily_disable():
                            try:
                                from packaging import tags
                            except ImportError:
                                # This is a subset of compatible tags, but these are the
                                # only ones that are particularly common; we need these
                                # to be able to install packaging and its dependencies.
                                generic_tags = [
                                    "py2.py3-none-any",
                                    "py3.py2-none-any",
                                    "py3-none-any",
                                ]

                                if match.group(1) not in generic_tags:
                                    continue
                            else:
                                if not cached_tags:
                                    cached_tags = set(AutoInstall.tags())

                                if all([tag not in cached_tags for tag in tags.parse_tag(match.group(1))]):
                                    continue

                        extension = 'whl'

                    else:
                        if package['name'].endswith(('.tar.gz', '.tar.bz2')):
                            extension = 'tar.gz'
                        elif package['name'].endswith('.zip'):
                            extension = 'zip'
                        else:
                            continue

                    requires = package.get('data-requires-python')
                    if requires and not AutoInstall.version.matches(requires):
                        continue

                    version_candidate = re.search(r'\d+\.\d+(\.\d+)?', package["name"])
                    if not version_candidate:
                        continue
                    version = Version(*version_candidate.group().split('.'))
                    if self.version and version != self.version:
                        continue

                    link = package['href'].split('#')[0]
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

            except (IOError, URLError) as e:
                if count > (AutoInstall.times_to_retry or 0):
                    raise
                else:
                    AutoInstall.log(str(e))
                    AutoInstall.log('Failed to download {}, retrying'.format(self.name))
            finally:
                if response:
                    response.close()
                count += 1

    def is_cached(self):
        manifest = AutoInstall.manifest.get(self.name)
        if not manifest:
            return False
        if AutoInstall.overwrite_foreign_packages and manifest.get('index') != AutoInstall.index:
            return False
        if not manifest.get('version'):
            return False
        if self.version and Version(*manifest.get('version').split('.')) not in self.version:
            return False
        if not all(pkg.is_cached() for dep in self.implicit_deps for pkg in AutoInstall.packages[dep]):
            return False
        return True

    def install(self):
        AutoInstall.register(self)
        if self.is_cached():
            return

        # Make sure that base libraries are installed, since setup.py relies on them
        if self.name not in AutoInstall.BASE_LIBRARIES:
            for library in AutoInstall.BASE_LIBRARIES:
                AutoInstall.install(library)

        # In some cases a package may check if another package is installed without actually
        # importing it, which would make the AutoInstall to miss the dependency as it would
        # not be imported. An example is pytest autoload feature to search for plugins like
        # pytest_timeout.
        for dependency in self.implicit_deps:
            AutoInstall.install(dependency)

        with FileLock(os.path.join(AutoInstall.directory, AutoInstall.LOCK_FILE), timeout=AutoInstall.LOCKFILE_TIMEOUT):
            # Re-load our manifest, in case another process is using this autoinstall location.
            # Another process running in parallel to this one is likely to be installing the same packages.
            try:
                with open(os.path.join(AutoInstall.directory, AutoInstall.MANIFEST_JSON), 'r') as file:
                    AutoInstall.manifest.update(json.load(file))
                if self.is_cached():
                    return
            except (IOError, OSError, ValueError):
                pass

            if not self.archives():
                raise ValueError('No archives for {}-{} found'.format(self.pypi_name, self.version))
            archive = self.archives()[-1]

            try:
                shutil.rmtree(self.location, ignore_errors=True)
                as_py_file = '{}.py'.format(self.location)
                if os.path.isfile(as_py_file):
                    os.remove(as_py_file)

                AutoInstall.log('Downloading {}...'.format(archive))
                archive.download()

                temp_location = os.path.join(tempfile.gettempdir(), '{}-{}'.format(self.name, os.getpid()))
                archive.unpack(temp_location)

                for candidate in os.listdir(temp_location):
                    candidate = os.path.join(temp_location, candidate)
                    if not os.path.exists(os.path.join(candidate, 'setup.py')):
                        # If a package has a setup.cfg, we can just lay down a dummy setup.py and let setuptools handle the rest
                        if os.path.exists(os.path.join(candidate, 'setup.cfg')):
                            with open(os.path.join(candidate, 'setup.py'), 'w') as setup_py:
                                setup_py.write('from setuptools import setup\n')
                                setup_py.write('if __name__ == "__main__":\n')
                                setup_py.write('    setup()\n')
                        else:
                            continue

                    AutoInstall.log('Installing {}...'.format(archive))

                    if self.slow_install:
                        AutoInstall.log('{} is known to be slow to install'.format(archive))

                    root_location = "/" if not sys.platform.startswith('win') else "{}/".format(os.path.splitdrive(os.path.abspath(AutoInstall.directory))[0])

                    log_location = os.path.join(temp_location, 'log.txt')
                    try:
                        with open(log_location, 'w') as setup_log:
                            subprocess.check_call(
                                [
                                    os.environ.get('AUTOINSTALL_PYTHON_EXECUTABLE', sys.executable),
                                    os.path.join(candidate, 'setup.py'),
                                    'install',
                                    '--home={}'.format(AutoInstall.directory),
                                    '--root={}'.format(root_location),
                                    '--prefix=',
                                    '--install-lib={}'.format(AutoInstall.directory),
                                    '--install-scripts={}'.format(os.path.join(AutoInstall.directory, 'bin')),
                                    '--install-data={}'.format(os.path.join(AutoInstall.directory, 'data')),
                                    '--install-headers={}'.format(os.path.join(AutoInstall.directory, 'headers')),
                                ],
                                cwd=candidate,
                                env=dict(
                                    HTTP_PROXY=os.environ.get('HTTP_PROXY', ''),
                                    HTTPS_PROXY=os.environ.get('HTTPS_PROXY', ''),
                                    PATH=os.environ.get('PATH', ''),
                                    PATHEXT=os.environ.get('PATHEXT', ''),
                                    PYTHONPATH=AutoInstall.directory,
                                    SYSTEMROOT=os.environ.get('SYSTEMROOT', ''),
                                ) if not sys.platform.startswith('win')
                                else dict(
                                    # Windows setuptools needs environment from vcvars
                                    os.environ,
                                    PYTHONPATH=AutoInstall.directory,
                                ),
                                stdout=setup_log,
                                stderr=setup_log,
                            )

                    except subprocess.CalledProcessError:
                        with open(log_location, 'r') as setup_log:
                            for line in setup_log.readlines():
                                sys.stderr.write(line)
                        raise

                    # If we have a package inside another package (like zope.interface), the top-level package needs an __init__.py
                    location = os.path.join(AutoInstall.directory, self.name.split('.')[0])
                    if os.path.isdir(location) and '__init__.py' not in os.listdir(location):
                        with open(os.path.join(location, '__init__.py'), 'w') as init:
                            init.write('\n')

                    break
                else:
                    # We might not need setup.py at all, check if we have dist-info and the library in the temporary location
                    to_be_moved = os.listdir(temp_location)
                    if (
                        archive.extension != 'whl'
                        or not any(
                            element.endswith('.dist-info') for element in to_be_moved
                        )
                    ):
                        raise OSError('Cannot install {}, could not find setup.py'.format(self.name))
                    for file in to_be_moved:
                        target = os.path.join(AutoInstall.directory, file)
                        if os.path.isdir(target):
                            shutil.rmtree(target, ignore_errors=True)
                        elif os.path.exists(target):
                            os.remove(target)
                        shutil.move(os.path.join(temp_location, file), AutoInstall.directory)

                self.do_post_install(temp_location)

                os.remove(archive.path)
                shutil.rmtree(temp_location, ignore_errors=True)

                AutoInstall.userspace_should_own(AutoInstall.directory)

                AutoInstall.manifest[self.name] = {
                    'index': AutoInstall.index,
                    'version': str(archive.version),
                }

                AutoInstall.log('Installed {}!'.format(archive))
            except Exception:
                if self.name in AutoInstall.manifest:
                    del AutoInstall.manifest[self.name]

                AutoInstall.log('Failed to install {}!'.format(archive), level=logging.CRITICAL)
                raise
            finally:
                manifest = os.path.join(AutoInstall.directory, AutoInstall.MANIFEST_JSON)
                with open(manifest, 'w') as file:
                    json.dump(AutoInstall.manifest, file, indent=4)
                AutoInstall.userspace_should_own(manifest)



def _default_pypi_index():
    pypi_url = re.compile(r'\Aindex\S* = https?://(?P<host>\S+)/.*')
    pip_config = '/Library/Application Support/pip/pip.conf'
    if os.path.isfile(pip_config):
        with open(pip_config, 'r') as config:
            for line in config.readlines():
                match = pypi_url.match(line.lstrip())
                if match:
                    return match.group('host')
    return 'pypi.org'


class AutoInstall(object):
    LOCKFILE_TIMEOUT = 5 * 60
    MANIFEST_JSON = 'manifest.json'
    LOCK_FILE = 'autoinstall.lock'
    DISABLE_ENV_VAR = 'DISABLE_WEBKITCOREPY_AUTOINSTALLER'
    CA_CERT_PATH_ENV_VAR = 'AUTOINSTALL_CA_CERT_PATH'

    # This list of libraries is required to install other libraries, and must be installed first
    BASE_LIBRARIES = ['setuptools', 'wheel', 'six', 'pyparsing', 'packaging', 'tomli', 'setuptools_scm']

    directory = None
    index = _default_pypi_index()
    timeout = 30
    times_to_retry = 1
    version = Version(sys.version_info[0], sys.version_info[1], sys.version_info[2])
    packages = defaultdict(list)
    manifest = {}

    # Rely on our own certificates for PyPi, since we use PyPi to standardize root certificates.
    # This is not needed in Linux platforms.
    ca_cert_path = os.environ.get(CA_CERT_PATH_ENV_VAR)
    if not ca_cert_path or not os.path.isfile(ca_cert_path):
        ca_cert_path = os.path.join(os.path.dirname(__file__), 'cacert.pem')

    _previous_index = None
    _previous_ca_cert_path = None
    _fatal_check = False
    _temporary_disable = 0

    # When sharing an install location, projects may wish to overwrite packages on disk
    # originating from a different index.
    overwrite_foreign_packages = False

    @classmethod
    def _request(cls, url, ca_cert_path=None):
        if sys.platform.startswith('linux'):
            return urlopen(url, timeout=cls.timeout)

        context = ssl.create_default_context(cafile=ca_cert_path or cls.ca_cert_path)
        return urlopen(url, timeout=cls.timeout, context=context)

    @classmethod
    def enabled(cls):
        if cls._temporary_disable > 0:
            return False
        if os.environ.get(cls.DISABLE_ENV_VAR) not in ['0', 'FALSE', 'False', 'false', 'NO', 'No', 'no', None]:
            return False
        return True if cls.directory else None

    @classmethod
    @contextmanager
    def temporarily_disable(cls):
        cls._temporary_disable += 1
        yield
        cls._temporary_disable -= 1

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
            with open(os.path.join(directory, cls.MANIFEST_JSON), 'r') as file:
                cls.manifest = json.load(file)
        except (IOError, OSError, ValueError):
            pass

        sys.path.insert(0, directory)
        cls.directory = directory

    @classmethod
    def _verify_index(cls):
        if not cls._previous_index:
            return

        def error(message):
            if cls._fatal_check:
                raise ValueError(message)

            sys.stderr.write('{}\n'.format(message))
            sys.stderr.write('Falling back to previous index, {}\n\n'.format(cls._previous_index))

            cls.index = cls._previous_index
            cls.ca_cert_path = cls._previous_ca_cert_path

        response = None
        try:
            response = AutoInstall._request('https://{}/simple/pip/'.format(cls.index), ca_cert_path=cls.ca_cert_path)
            if response.code != 200:
                error('Failed to set AutoInstall index to {}, received {} response when searching for simple/pip'.format(index, response.code))

        except URLError:
            error('Failed to set AutoInstall index to {}, no response from the server'.format(cls.index))

        finally:
            if response:
                response.close()

            cls._previous_index = None
            cls._previous_ca_cert_path = None
            cls._fatal_check = False

    @classmethod
    def set_index(cls, index, check=False, fatal=False, ca_cert_path=None):
        cls._previous_index = cls.index
        cls._previous_ca_cert_path = cls.ca_cert_path
        cls._fatal_check = fatal

        cls.index = index
        cls.ca_cert_path = ca_cert_path

        if check:
            cls._verify_index()

        if cls.ca_cert_path:
            os.environ[cls.CA_CERT_PATH_ENV_VAR] = ca_cert_path

        return cls.index

    @classmethod
    def set_timeout(cls, timeout):
        if timeout is not None and timeout <= 0:
            raise ValueError('{} is an invalid timeout value'.format(timeout))
        cls.timeout = math.ceil(timeout)

    @classmethod
    def register(cls, package, local=False):
        if isinstance(package, Package):
            if cls.packages.get(package.name):
                if cls.packages.get(package.name)[0].version != package.version:
                    raise ValueError('Registered version of {} uses {}, but requested version uses {}'.format(package.name, cls.packages.get(package.name)[0].version, package.version))
                return cls.packages.get(package.name)
        else:
            raise ValueError('Expected package to be Package, not {}'.format(type(package)))

        if not isinstance(local, bool):
            raise ValueError('Expected local to be bool, not {}'.format(type(local)))

        # If inside the WebKit checkout, a local library is likely checked in at Tools/Scripts/libraries.
        # When we detect such a library, we should not register it to be auto-installed
        if local:
            if package.name == 'autoinstalled':
                raise ValueError("local package name 'autoinstalled' is forbidden")
            containing_path = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
            libraries = os.path.dirname(containing_path)
            checkout_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(libraries))))
            for candidate in [
                containing_path,
                os.path.join(libraries, package.pypi_name),
                os.path.join(checkout_root, 'Internal', 'Tools', 'Scripts', 'libraries', package.pypi_name),
            ]:
                if not os.path.isdir(os.path.join(candidate, package.name)):
                    continue
                if candidate in sys.path:
                    return [package]
                sys.path.insert(0, candidate)
                return [package]
            else:
                raise ValueError("unable find local package {}".format(package.name))
        assert not local  # this should follow from the above

        if package.version is None:
            raise ValueError("trying to install non-local package {} with unspecified version".format(package.name))

        for alias in package.aliases:
            cls.packages[alias].append(package)
        cls.packages[package.name].append(package)
        return [package]

    @classmethod
    def install(cls, package):
        if not cls.enabled():
            sys.stderr.write("Autoinstaller disabled, but 'install' called\n")
            return None
        if isinstance(package, str):
            # we want this to throw if it hasn't been previously registered; in the case
            # that this is being called from cls.find_module it should always exist
            packages = cls.packages[package]
        else:
            packages = cls.register(package)
        return all([to_install.install() for to_install in packages])

    @classmethod
    def install_everything(cls):
        # Iterate over a copy, as implicit_deps can lead to new packages being
        # registered during installation
        for packages in list(cls.packages.values()):
            for package in packages:
                package.install()
        return None

    @classmethod
    def find_spec(cls, fullname, path=None, target=None):
        loader = cls.find_module(fullname, path=path)
        if not loader:
            return None
        return loader.create_module(None)

    @classmethod
    def find_module(cls, fullname, path=None):
        if not cls.enabled() or path is not None:
            return None

        name = fullname.split('.')[0]
        if not cls.packages.get(name) or not cls.directory:
            return None

        cls.install(name)

        path = cls.directory
        for part in fullname.split('.'):
            path = os.path.join(path, part)
            for ext in ('', '.py', '.pyc', '.py3', '.pyo'):
                candidate = '{}{}'.format(path, ext)
                if os.path.exists(candidate):
                    path = candidate
                    break
            if not os.path.isdir(path):
                break
        if os.path.isdir(path):
            path = os.path.join(path, '__init__.py')

        return importmachinery.SourceFileLoader(name, path)

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

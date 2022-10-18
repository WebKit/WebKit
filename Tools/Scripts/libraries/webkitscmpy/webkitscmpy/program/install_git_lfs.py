# Copyright (C) 2022 Apple Inc. All rights reserved.
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

import logging
import os
import platform
import re
import requests
import shutil
import subprocess
import sys
import tempfile
import zipfile

from webkitcorepy import Version, arguments, run
from webkitscmpy import local, log
from .command import Command


class InstallGitLFS(Command):
    name = 'install-git-lfs'
    help = "Install 'git lfs' on this machine"

    BASE_URL = 'https://github.com/git-lfs/git-lfs/releases/download'
    VERSION = Version(3, 1, 2)
    FILE = 'git-lfs-v{}.zip'.format(VERSION)

    VERSION_RE = re.compile(r'^git-lfs/(?P<version>\d+\.\d+\.\d+) \(.*\)')

    @classmethod
    def url(cls, mirror_resolver_func=None):
        _url = None
        from whichcraft import which
        if platform.system() == 'Darwin':
            # x86_64 compiled version works better with VPN, if Rosetta is installed, prefer it even on Apple Silicon
            if platform.machine() == 'arm64' and not which('rosetta'):
                _url = '{url}/v{version}/git-lfs-darwin-arm64-v{version}.zip'.format(url=cls.BASE_URL, version=cls.VERSION)
            else:
                _url = '{url}/v{version}/git-lfs-darwin-amd64-v{version}.zip'.format(url=cls.BASE_URL, version=cls.VERSION)
        _url = mirror_resolver_func(_url) if _url and callable(mirror_resolver_func) else _url
        # FIXME: Determine URLs for non-Darwin installs
        return _url

    @classmethod
    def install(cls, mirror_resolver_func=None):
        url = cls.url(mirror_resolver_func=mirror_resolver_func)
        if not url:
            sys.stderr.write('No `git lfs` install implemented for the current platform\n')
            return False

        log.info("Downloading `git lfs` from '{}'...".format(url))
        response = requests.get(url, allow_redirects=True)
        log.info("Downloading finished, status code '{}'".format(response.status_code))

        if response.status_code != 200:
            sys.stderr.write("Failed to download `git lfs` from '{}'\n".format(cls.url()))
            sys.stderr.write("Response code '{}'\n".format(response.status_code))
            return False

        tmpdir = tempfile.mkdtemp()
        try:
            dest = '{}/{}'.format(tmpdir, cls.FILE)
            log.info("Saving `git lfs` to '{}'".format(dest))
            with open(dest, 'wb') as file:
                file.write(response.content)

            target = os.path.splitext(dest)[0]
            log.info("Unpacking '{}'...".format(dest))
            with zipfile.ZipFile(dest, 'r') as file:
                file.extractall(target)
            log.info("Unpacked '{}' to '{}'...".format(dest, target))

            log.info("Installing `git lfs` from '{}'...".format(target))
            install_script = os.path.join(target, 'install.sh')
            if not os.path.isfile(install_script):
                sys.stderr.write("Could not find install script at '{}'\n".format(install_script))
                return False
            result = run(
                ['sudo', 'sh', install_script],
                cwd=os.path.dirname(install_script),
                stdout=None if log.getEffectiveLevel() > logging.DEBUG else subprocess.PIPE,
            )
            log.info("`git lfs` install finished with exit code '{}'".format(result.returncode))
            if result.returncode:
                sys.stderr.write("Failed `git lfs` install with exit code '{}'\n".format(result.returncode))
                return False
            return True

        finally:
            shutil.rmtree(tmpdir, ignore_errors=True)

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            '--overwrite', '-o', '--force', '-f',
            help='Overwrite existing `git lfs` install, regardless of version',
            action='store_true',
            dest='overwrite',
            default=False,
        )
        parser.add_argument(
            '--no-configure', '--configure',
            help='Explicitly opt out of configuring this checkout for `git lfs`',
            action=arguments.NoAction,
            dest='configure',
            default=None,
        )
        parser.set_defaults(
            mirror_resolver_func=None,
        )


    @classmethod
    def main(cls, args, repository, **kwargs):
        if repository and not repository.path:
            sys.stderr.write("Cannot install `git lfs` from a remote repository\n")
            return 1

        if repository and not isinstance(repository, local.Git):
            sys.stderr.write("`git lfs` only supported from git repositories\n")
            return 1

        if args.configure and not repository:
            sys.stderr.write('Cannot configure `git lfs` without a repository\n')
            return 1

        log.info('Checking `git lfs` version')
        ran = run([local.Git.executable(), 'lfs', '--version'], capture_output=True, encoding='utf-8')
        match = cls.VERSION_RE.match(ran.stdout)
        version = Version.from_string(match.group('version')) if match else None

        if args.overwrite or ran.returncode or version != cls.VERSION:
            print('Installing `git lfs` version {}'.format(cls.VERSION))
            if not cls.install(mirror_resolver_func=args.mirror_resolver_func):
                sys.stderr.write("Failed to install `git lfs` on this machine\n")
                return 1
        else:
            print("`git lfs` version {} is already installed".format(version))

        if not repository:
            print("No repository provided, skipping configuring `git lfs`")
            return 0
        if args.configure is False:
            print("Skipping configuring `git lfs` for '{}', as requested".format(repository.path))
            return 0

        lfs_repo_status = repository.config(location='repository').get('lfs.repositoryformatversion')
        if not args.overwrite and lfs_repo_status == '0':
            print("`git lfs` is already configured for '{}'".format(repository.path))
            return 0
        log.info("Configuring `git lfs` for '{}'".format(repository.path))
        if run([repository.executable(), 'lfs', 'install'], capture_output=log.getEffectiveLevel() > logging.INFO).returncode:
            sys.stderr.write("Failed to configure `git lfs` for '{}'\n".format(repository.path))
            return 1
        print("Configured `git lfs` for '{}'".format(repository.path))
        return 0

# Copyright (C) 2023 Apple Inc. All rights reserved.
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

import os
import pprint
import re
import shutil
import sys

from webkitbugspy import radar
from webkitcorepy import Terminal, Version, run, string_utils

from webkitscmpy import local, log, remote

from .command import Command


class InstallHooks(Command):
    name = 'install-hooks'
    help = 'Re-install all hooks from this repository into this checkout'

    REMOTE_RE = re.compile(r'(?P<protcol>[^@:]+://)?(?P<user>[^:@]+@)?(?P<host>[^:/@]+)(/|:)(?P<path>[^\.]+[^\./])(\.git)?/?')
    VERSION_RE = re.compile(r'^VERSION\s*=\s*[\'"](?P<number>\d+(\.\d+)*)[\'"]$')
    MODES = ('default', 'publish', 'no-radar')

    @classmethod
    def version_for(cls, path):
        if not os.path.isfile(path):
            return None
        with open(path, 'r') as f:
            for line in f.readlines():
                match = cls.VERSION_RE.match(line)
                if match:
                    return Version.from_string(match.group('number'))
        return None

    @classmethod
    def hook_needs_update(cls, repository, path):
        if not os.path.isfile(path):
            return False

        hooks_directory_path = repository.config().get('core.hookspath', os.path.join(repository.common_directory, 'hooks'))
        hook_path = os.path.join(hooks_directory_path, os.path.basename(path))
        if not os.path.isfile(hook_path):
            return True
        repo_version = cls.version_for(path)
        if not repo_version:
            return False
        installed_version = cls.version_for(hook_path)
        if not repo_version:
            return True
        return repo_version > installed_version

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'arguments', nargs='*',
            type=str, default=None,
            help='Name of specific hooks to be installed',
        )
        parser.add_argument(
            '--mode', default=cls.MODES[0],
            help='Set default mode for installed hooks ({})'.format(string_utils.join(cls.MODES, conjunction='or')),
        )
        parser.add_argument(
            '--level', type=str, default=[], action='append', dest='levels',
            help="Specify a security level for a specific remote with '<hostname>:<path>=<level>'. Note that this "
                 "level cannot override the level specified by the repository's for the same remote."
        )

    @classmethod
    def _security_levels(cls, repository):
        proc = run(
            [local.Git.executable(), 'config', '--get-regexp', 'webkitscmpy.remotes'],
            capture_output=True, cwd=repository.root_path,
            encoding='utf-8',
        )
        if proc.returncode:
            return {}

        levels_by_name = {}
        remotes_by_name = {}
        for line in proc.stdout.splitlines():
            key, value = line.split(' ', 1)
            parts = key.split('.')
            if len(parts) != 4:
                continue
            if parts[3] == 'url':
                remotes_by_name[parts[2]] = value
            elif parts[3] == 'security-level':
                levels_by_name[parts[2]] = int(value)

        result = {}
        for name, rmt in remotes_by_name.items():
            match = cls.REMOTE_RE.match(rmt)
            if match:
                result['{}:{}'.format(match.group('host'), match.group('path')).lower()] = levels_by_name.get(name, None)

        proc = run(
            [local.Git.executable(), 'config', '--get-regexp', 'remote.+url'],
            capture_output=True, cwd=repository.root_path,
            encoding='utf-8',
        )
        if proc.returncode:
            return result
        for line in proc.stdout.splitlines():
            _, value = line.split(' ', 1)
            match = cls.REMOTE_RE.match(value)
            if not match:
                continue
            key = '{}:{}'.format(match.group('host'), match.group('path')).lower()
            if key in result:
                continue
            repo = 'https://{}/{}'.format(match.group('host'), match.group('path'))
            if not remote.GitHub.is_webserver(repo):
                continue
            parent = ((remote.GitHub(repo).request() or {}).get('parent') or {}).get('full_name')
            if not parent:
                continue
            parent_key = '{}:{}'.format(match.group('host'), parent).lower()
            if parent_key in result:
                result[key] = result[parent_key]
        return result

    @classmethod
    def main(cls, args, repository, hooks=None, identifier_template=None, **kwargs):
        if not isinstance(repository, local.Git):
            sys.stderr.write('Can only install hooks in a native git repository\n')
            return 1
        if not hooks:
            sys.stderr.write('No hooks to install\n')
            return 1

        candidates = os.listdir(hooks)
        if getattr(args, 'arguments', []):
            hook_names = []
            for argument in args.arguments:
                if argument in candidates:
                    hook_names.append(argument)
                else:
                    sys.stderr.write("'{}' is not a configured hook in this repository\n".format(argument))
        else:
            hook_names = candidates

        if not hook_names:
            sys.stderr.write('No matching hooks in repository to install\n')
            return 1

        early_exit = False
        security_levels = cls._security_levels(repository)
        for level in getattr(args, 'levels', None) or []:
            key, value = level.split('=', 1)
            if not re.match(r'\d+', value):
                sys.stderr.write("'{}' is not a valid security level\n".format(value))
                continue
            value = int(value)
            if key.lower() in security_levels:
                exisiting_level = security_levels.get(key.lower(), None)
                if exisiting_level is None:
                    sys.stderr.write("'{}' is already specified, but with an unknown security level\n".format(key))
                if exisiting_level != value:
                    sys.stderr.write("'{}' already has a security level of '{}'\n".format(key, exisiting_level))
                    early_exit = True
                    continue
            security_levels[key.lower()] = value
        if early_exit:
            return 1

        # Ensure security_levels has the readable, increasing, order.
        security_levels = dict(
            sorted(security_levels.items(), key=lambda x: (x[1], x[0]))
        )

        trailers_to_strip = ['Identifier'] + ([identifier_template.split(':', 1)[0]] if identifier_template else [])
        source_remotes = repository.source_remotes() or ['origin']
        perl = 'perl'
        if sys.version_info >= (3, 3):
            perl = shutil.which('perl')

        default_target_directory = os.path.join(repository.common_directory, 'hooks')
        target_directory = repository.config().get('core.hookspath', default_target_directory)
        if default_target_directory != target_directory:
            response = Terminal.choose(
                "git believes hooks for this repository are in '{}', but they should be in '{}'".format(target_directory, default_target_directory),
                options=('Change', 'Use', 'Exit'),
                default='Change',
            )
            if response == 'Exit':
                sys.stderr.write('No hooks installed because user canceled hook installation\n')
                return 1
            elif response == 'Change':
                if run(
                    [local.Git.executable(), 'config', 'core.hookspath', default_target_directory],
                    capture_output=True, cwd=repository.root_path,
                ).returncode:
                    sys.stderr.write('No hooks installed')
                    sys.stderr.write("Failed to set 'core.hookspath' to '{}'\n".format(default_target_directory))
                    return 1
                target_directory = default_target_directory

        installed_hooks = 0
        for hook in hook_names:
            source_path = os.path.join(hooks, hook)
            if not os.path.isfile(source_path):
                continue
            target = os.path.join(target_directory, hook)
            if os.path.islink(target):
                sys.stderr.write("'{}' is a symlink, refusing to overwrite it\n".format(hook))
                continue

            log.info("Configuring and copying hook '{}' for this repository".format(hook))
            from jinja2 import Template

            with open(source_path, 'r') as f:
                contents = Template(f.read()).render(
                    location=os.path.relpath(source_path, repository.root_path),
                    perl=repr(perl),
                    python=os.path.basename(sys.executable),
                    prefer_radar=bool(radar.Tracker.radarclient()),
                    default_pre_push_mode=repr(str(getattr(args, 'mode', cls.MODES[0]))),
                    security_levels=pprint.pformat(security_levels, indent=1, width=1, sort_dicts=False),
                    remote_re=cls.REMOTE_RE.pattern,
                    default_branch=repository.default_branch,
                    trailers_to_strip=trailers_to_strip,
                    source_remotes=source_remotes,
                )

            if not os.path.exists(os.path.dirname(target)):
                os.makedirs(os.path.dirname(target))
            with open(target, 'w') as f:
                f.write(contents)
                f.write('\n')
            os.chmod(target, 0o775)
            installed_hooks += 1

        print('Successfully installed {} of {} repository hooks'.format(installed_hooks, len(hook_names)))
        if installed_hooks != len(hook_names):
            print('    {} repository hooks skipped because of existing symlinks'.format(len(hook_names) - installed_hooks))
        return 0

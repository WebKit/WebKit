# Copyright (C) 2021 Apple Inc. All rights reserved.
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
import requests
import sys

from .command import Command
from jinja2 import Template
from requests.auth import HTTPBasicAuth
from webkitcorepy import arguments, run, Editor, Terminal
from webkitscmpy import log, local, remote


class Setup(Command):
    name = 'setup'
    help = 'Configure local settings for the current repository'

    @classmethod
    def github(cls, args, repository, additional_setup=None, **kwargs):
        log.warning('Saving GitHub credentials in system credential store...')
        username, access_token = repository.credentials(required=True)
        log.warning('GitHub credentials saved via Keyring!')

        # Any additional setup passed to main
        result = 0
        if additional_setup:
            result += additional_setup(args, repository)

        log.warning('Verifying user owned fork...')
        auth = HTTPBasicAuth(username, access_token)
        response = requests.get('{}/repos/{}/{}'.format(
            repository.api_url,
            username,
            repository.name,
        ), auth=auth, headers=dict(Accept='application/vnd.github.v3+json'))
        if response.status_code == 200:
            log.warning("User already owns a fork of '{}'!".format(repository.name))
            return result

        if repository.owner == username or args.defaults or Terminal.choose(
            "Create a private fork of '{}' belonging to '{}'".format(repository.name, username),
            default='No',
        ) == 'No':
            log.warning("Continuing without forking '{}'".format(repository.name))
            return 1

        response = requests.post('{}/repos/{}/{}/forks'.format(
            repository.api_url,
            repository.owner,
            repository.name,
        ), json=dict(
            owner=username,
            description="{}'s fork of {}".format(username, repository.name),
            private=True,
        ), auth=auth, headers=dict(Accept='application/vnd.github.v3+json'))
        if response.status_code not in (200, 202):
            sys.stderr.write("Failed to create a fork of '{}' belonging to '{}'\n".format(repository.name, username))
            return 1
        log.warning("Created a private fork of '{}' belonging to '{}'!".format(repository.name, username))
        return result

    @classmethod
    def git(cls, args, repository, additional_setup=None, hooks=None, **kwargs):
        global_config = local.Git.config()
        result = 0

        email = global_config.get('user.email')
        log.warning('Setting git user email for {}...'.format(repository.root_path))
        if not email or args.defaults is False or (not args.defaults and Terminal.choose(
            "Set '{}' as the git user email".format(email),
            default='Yes',
        ) == 'No'):
            email = Terminal.input('Git user email: ')

        if run(
            [local.Git.executable(), 'config', 'user.email', email], capture_output=True, cwd=repository.root_path,
        ).returncode:
            sys.stderr.write('Failed to set the git user email to {}\n'.format(email))
            result += 1
        else:
            log.warning("Set git user email to '{}'".format(email))

        name = repository.contributors.get(email)
        if name:
            name = name.name
        else:
            name = global_config.get('user.name')
        log.warning('Setting git user name for {}...'.format(repository.root_path))
        if not name or args.defaults is False or (not args.defaults and Terminal.choose(
            "Set '{}' as the git user name".format(name),
            default='Yes',
        ) == 'No'):
            name = Terminal.input('Git user name: ')
        if run(
            [local.Git.executable(), 'config', 'user.name', name], capture_output=True, cwd=repository.root_path,
        ).returncode:
            sys.stderr.write('Failed to set the git user name to {}\n'.format(name))
            result += 1
        else:
            log.warning("Set git user name to '{}'".format(name))

        log.warning('Setting better Objective-C diffing behavior...')
        result += run(
            [local.Git.executable(), 'config', 'diff.objcpp.xfuncname', '^[-+@a-zA-Z_].*$'],
            capture_output=True, cwd=repository.root_path,
        ).returncode
        result += run(
            [local.Git.executable(), 'config', 'diff.objcppheader.xfuncname', '^[@a-zA-Z_].*$'],
            capture_output=True, cwd=repository.root_path,
        ).returncode
        log.warning('Set better Objective-C diffing behavior!')

        if args.defaults or Terminal.choose(
            'Auto-color status, diff, and branch?',
            default='Yes',
        ) == 'Yes':
            for command in ('status', 'diff', 'branch'):
                result += run(
                    [local.Git.executable(), 'config', 'color.{}'.format(command), 'auto'],
                    capture_output=True, cwd=repository.root_path,
                ).returncode

        log.warning('Using {} merge strategy'.format('merge commits as a' if args.merge else 'a rebase'))
        if run(
            [local.Git.executable(), 'config', 'pull.rebase', 'false' if args.merge else 'true'],
            capture_output=True, cwd=repository.root_path,
        ).returncode:
            sys.stderr.write('Failed to use {} as the merge strategy\n'.format('merge commits' if args.merge else 'rebase'))
            result += 1

        if hooks:
            for hook in os.listdir(hooks):
                source_path = os.path.join(hooks, hook)
                if not os.path.isfile(source_path):
                    continue
                log.warning('Configuring and copying hook {}'.format(source_path))
                with open(source_path, 'r') as f:
                    contents = Template(f.read()).render(
                        git=local.Git.executable(),
                        location=source_path,
                        python=os.path.basename(sys.executable),
                    )

                target = os.path.join(repository.root_path, '.git', 'hooks', hook)
                if not os.path.exists(os.path.dirname(target)):
                    os.makedirs(os.path.dirname(target))
                with open(target, 'w') as f:
                    f.write(contents)
                    f.write('\n')
                os.chmod(target, 0o775)

        log.warning('Setting git editor for {}...'.format(repository.root_path))
        editor_name = 'default' if args.defaults else Terminal.choose(
            'Pick a commit message editor',
            options=['default'] + [program.name for program in Editor.programs()],
            default='default',
            numbered=True,
        )
        if editor_name == 'default':
            log.warning('Using the default git editor')
        elif run(
            [local.Git.executable(), 'config', 'core.editor', ' '.join([arg.replace(' ', '\\ ') for arg in Editor.by_name(editor_name).wait])],
            capture_output=True,
            cwd=repository.root_path,
        ).returncode:
            sys.stderr.write('Failed to set the git editor to {}\n'.format(editor_name))
            result += 1
        else:
            log.warning("Set git editor to '{}'".format(editor_name))

        # Pushing to http repositories is difficult, offer to change http checkouts to ssh
        http_remote = local.Git.HTTP_REMOTE.match(repository.url())
        if http_remote and not args.defaults and Terminal.choose(
            "http based remotes will prompt for your password when pushing,\nwould you like to convert to a ssh remote?",
            default='Yes',
        ) == 'Yes':
            if run([
                local.Git.executable(), 'config', 'remote.origin.url',
                'git@{}:{}.git'.format(http_remote.group('host'), http_remote.group('path')),
            ], capture_output=True, cwd=repository.root_path).returncode:
                sys.stderr.write("Failed to change remote to ssh remote '{}'\n".format(
                    'git@{}:{}.git'.format(http_remote.group('host'), http_remote.group('path'))
                ))
                result += 1
            else:
                # Force reset cache
                repository.url(cached=False)

        # Any additional setup passed to main
        if additional_setup:
            result += additional_setup(args, repository)

        # Only configure GitHub if the URL is a GitHub URL
        rmt = repository.remote()
        if not isinstance(rmt, remote.GitHub):
            return result

        code = cls.github(args, rmt, **kwargs)
        result += code
        if code:
            return result

        username, _ = rmt.credentials(required=True)
        log.warning("Adding forked remote as '{}' and 'fork'...".format(username))
        url = repository.url()

        if '://' in url:
            fork_remote = '{}://{}/{}/{}.git'.format(url.split(':')[0], url.split('/')[2], username, rmt.name)
        elif ':' in url:
            fork_remote = '{}:{}/{}.git'.format(url.split(':')[0], username, rmt.name)
        else:
            return result + 1

        available_remotes = []
        for name in [username, 'fork']:
            returncode = run(
                [repository.executable(), 'remote', 'add', name, fork_remote],
                capture_output=True, cwd=repository.root_path,
            ).returncode
            if returncode == 3:
                returncode = run(
                    [repository.executable(), 'remote', 'set-url', name, fork_remote],
                    capture_output=True, cwd=repository.root_path,
                ).returncode
            if returncode:
                sys.stderr.write("Failed to add remote '{}'\n".format(name))
                result += 1
            else:
                available_remotes.append(name)
                log.warning("Added remote '{}'".format(name))

        if 'fork' not in available_remotes:
            return result
        log.warning("Fetching '{}'".format(fork_remote))
        return run(
            [repository.executable(), 'fetch', 'fork'],
            capture_output=True, cwd=repository.root_path,
        ).returncode

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            '--defaults', '--no-defaults', action=arguments.NoAction, default=None,
            help='Do not prompt the user for defaults, always use (or do not use) them',
        )
        parser.add_argument(
            '--merge', '--no-merge', action=arguments.NoAction, default=False,
            help='Use a merge-commit workflow instead of a rebase workflow',
        )

    @classmethod
    def main(cls, args, repository, **kwargs):
        if isinstance(repository, local.Git):
            return cls.git(args, repository, **kwargs)

        if isinstance(repository, remote.GitHub):
            return cls.github(args, repository, **kwargs)

        sys.stderr.write('No setup required for {}\n'.format(
            getattr(repository, 'root_path', getattr(repository, 'url', '?')),
        ))
        return 1

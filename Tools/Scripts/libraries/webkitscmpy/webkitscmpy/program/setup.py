# Copyright (C) 2021, 2022 Apple Inc. All rights reserved.
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

import getpass
import os
import requests
import sys
import time

from .command import Command
from requests.auth import HTTPBasicAuth
from webkitbugspy import radar
from webkitcorepy import arguments, run, Editor, OutputCapture, Terminal
from webkitscmpy import log, local, remote


class Setup(Command):
    name = 'setup'
    help = 'Configure local settings for the current repository'

    @classmethod
    def github(cls, args, repository, additional_setup=None, remote=None, team=None, **kwargs):
        log.info('Saving GitHub credentials in system credential store...')
        username, access_token = repository.credentials(required=True, validate=True, save_in_keyring=True)
        log.info('GitHub credentials saved via Keyring!')

        # Any additional setup passed to main
        result = 0
        if additional_setup:
            result += additional_setup(args, repository)

        team_id = None
        auth = HTTPBasicAuth(username, access_token)
        if team:
            response = requests.get('{}/orgs/{}'.format(
                repository.api_url,
                team,
            ), auth=auth, headers=dict(Accept=repository.ACCEPT_HEADER))
            if response.status_code == 200:
                team_id = response.json().get('id', None)
            else:
                sys.stderr.write('You are not a member of {}\n'.format(team))
                sys.stderr.write('Created "{}" fork will not be accessible to other contributors\n'.format(remote))

        forked_name = '{}{}'.format(repository.name, '-{}'.format(remote) if remote and not repository.name.endswith(remote) else '')
        log.info('Verifying user owned fork...')
        response = requests.get('{}/repos/{}/{}'.format(
            repository.api_url,
            username,
            forked_name,
        ), auth=auth, headers=dict(Accept=repository.ACCEPT_HEADER))

        if response.status_code == 200:
            has_team = False
            if team_id:
                log.info("Checking if '{}' has access to '{}/{}'...".format(team, username, forked_name))
                teams = requests.get('{}/repos/{}/{}/teams'.format(
                    repository.api_url,
                    username,
                    forked_name,
                ), auth=auth, headers=dict(Accept=repository.ACCEPT_HEADER))
                if teams.status_code == 200 and any([team_id == node.get('id') for node in teams.json()]):
                    log.info("'{}' has access to '{}/{}'!".format(team, username, forked_name))
                    has_team = True

            if not has_team and team:
                log.info("Granting '{}' access to '{}/{}'...".format(team, username, forked_name))
                granting = requests.put('{}/orgs/{}/repos/{}/{}'.format(
                    repository.api_url,
                    team,
                    username,
                    forked_name,
                ), json=dict(permission='push'), auth=auth, headers=dict(Accept=repository.ACCEPT_HEADER))
                if granting.status_code // 100 != 2:
                    sys.stderr.write("Failed to grant '{}' access to '{}/{}'\n".format(team, username, forked_name))
                    sys.stderr.write("Other contributors do not have access to '{}/{}'\n".format(username, forked_name))
                else:
                    log.info("Granted '{}' access to '{}/{}'!".format(team, username, forked_name))

            parent_name = response.json().get('parent', {}).get('full_name', None)
            if parent_name == '{}/{}'.format(repository.owner, repository.name):
                log.info("User already owns a fork of '{}'!".format(parent_name))
                return result

        if repository.owner == username or args.defaults or Terminal.choose(
            "Create a private fork of '{}' belonging to '{}'".format(forked_name, username),
            default='Yes',
        ) == 'No':
            log.info("Continuing without forking '{}'".format(forked_name))
            return 1

        data = dict(
            owner=username,
            description="{}'s fork of {}{}".format(username, repository.name, ' ({})'.format(remote) if remote else ''),
            private=True,
        )
        if team_id:
            data[team_id] = team_id
        response = requests.post('{}/repos/{}/{}/forks'.format(
            repository.api_url,
            repository.owner,
            repository.name,
        ), json=data, auth=auth, headers=dict(Accept=repository.ACCEPT_HEADER))
        if response.status_code // 100 != 2:
            sys.stderr.write("Failed to create a fork of '{}' belonging to '{}'\n".format(forked_name, username))
            sys.stderr.write("URL: {}\nServer replied with status code {}:\n{}\n".format(response.url, response.status_code, response.text))
            return 1

        set_name = response.json().get('name', '')
        if set_name and set_name != forked_name:
            response = requests.patch('{}/repos/{}/{}'.format(
                repository.api_url,
                username,
                set_name,
            ), json=dict(name=forked_name), auth=auth, headers=dict(Accept=repository.ACCEPT_HEADER))
            if response.status_code // 100 != 2:
                sys.stderr.write("Fork created with name '{}' belonging to '{}'\n Failed to change name to {}\n".format(set_name, username, forked_name))
                sys.stderr.write("URL: {}\nServer replied with status code {}:\n{}\n".format(response.url, response.status_code, response.text))
                return 1

        response = None
        attempts = 0
        while response and response.status_code // 100 != 2:
            if attempts > 3:
                sys.stderr.write("Waiting on '{}' belonging to '{}' took to long\n".format(forked_name, username))
                sys.stderr.write("Wait until '{}/{}/{}' is accessible and then re-run setup\n".format(
                    '/'.join(repository.url.split('/')[:3]),
                    username, forked_name,
                ))
                return 1
            if response:
                log.warning("Waiting for '{}' belonging to '{}'...".format(forked_name, username))
                time.sleep(10)
            attempts += 1
            response = requests.get('{}/repos/{}/{}'.format(
                repository.api_url,
                username,
                forked_name,
            ), auth=auth, headers=dict(Accept='application/vnd.github.v3+json'))

        log.info("Created a private fork of '{}' belonging to '{}'!".format(forked_name, username))
        return result

    @classmethod
    def _add_remote(cls, repository, name, url, fetch=True):
        returncode = run(
            [repository.executable(), 'remote', 'add', name, url],
            capture_output=True, cwd=repository.root_path,
        ).returncode
        if returncode == 3:
            returncode = run(
                [repository.executable(), 'remote', 'set-url', name, url],
                capture_output=True, cwd=repository.root_path,
            ).returncode
        if returncode:
            sys.stderr.write("Failed to add remote '{}'\n".format(name))
            return 1

        log.info("Added remote '{}'".format(name))

        if not fetch:
            return 0

        returncode = run(
            [repository.executable(), 'fetch', name],
            capture_output=True, cwd=repository.root_path,
        ).returncode
        if returncode:
            sys.stderr.write("Failed to fetch added remote '{}'\n".format(name))
            return 1
        log.info("Fetched remote '{}'".format(name))
        return 0

    @classmethod
    def _fork_remote(cls, origin, username, name):
        if '://' in origin:
            return '{}://{}/{}/{}.git'.format(origin.split(':')[0], origin.split('/')[2], username, name)
        elif ':' in origin:
            return '{}:{}/{}.git'.format(origin.split(':')[0], username, name)
        return None

    @classmethod
    def git(cls, args, repository, additional_setup=None, hooks=None, **kwargs):
        local_config = repository.config()
        global_config = local.Git.config()
        result = 0

        email = os.environ.get('EMAIL_ADDRESS') or local_config.get('user.email') or global_config.get('user.email')
        log.info('Setting git user email for {}...'.format(repository.root_path))
        if not email or args.defaults is False or (not args.defaults and args.all and Terminal.choose(
            "Set '{}' as the git user email for this repository".format(email),
            default='Yes',
        ) == 'No'):
            email = Terminal.input('Enter git user email for this repository: ')

        if email != local_config.get('user.email'):
            if run(
                [local.Git.executable(), 'config', 'user.email', email], capture_output=True, cwd=repository.root_path,
            ).returncode:
                sys.stderr.write('Failed to set the git user email to {} for this repository\n'.format(email))
                result += 1
            else:
                log.info("Set git user email to '{}' for this repository".format(email))
        else:
            log.info("Skipped setting email to '{}', it's already set for this repository".format(email))

        contributor = repository.contributors.get(email)
        if contributor:
            name = contributor.name
        else:
            name = local_config.get('user.name') or global_config.get('user.name')
        log.info('Setting git user name for {}...'.format(repository.root_path))
        if not name or args.defaults is False or (not args.defaults and args.all and Terminal.choose(
            "Set '{}' as the git user name for this repository".format(name),
            default='Yes',
        ) == 'No'):
            name = Terminal.input('Enter git user name for this repository: ')

        if name != local_config.get('user.name'):
            if run(
                [local.Git.executable(), 'config', 'user.name', name], capture_output=True, cwd=repository.root_path,
            ).returncode:
                sys.stderr.write('Failed to set the git user name to {} for this repository\n'.format(name))
                result += 1
            else:
                log.info("Set git user name to '{}' for this repository".format(name))
        else:
            log.info("Skipped setting name to '{}', it's already set for this repository".format(name))

        if repository.metadata and os.path.isfile(os.path.join(repository.metadata, local.Git.GIT_CONFIG_EXTENSION)):
            log.info('Adding project git config to repository config...')
            result += run(
                [local.Git.executable(), 'config', 'include.path', os.path.join('..', os.path.basename(repository.metadata), local.Git.GIT_CONFIG_EXTENSION)],
                capture_output=True, cwd=repository.root_path,
            ).returncode
            log.info('Added project git config to repository config!')
        else:
            log.info('No project git config found, continuing')

        log.info('Setting better Objective-C diffing behavior for this repository...')
        result += run(
            [local.Git.executable(), 'config', 'diff.objcpp.xfuncname', '^[-+@a-zA-Z_].*$'],
            capture_output=True, cwd=repository.root_path,
        ).returncode
        result += run(
            [local.Git.executable(), 'config', 'diff.objcppheader.xfuncname', '^[@a-zA-Z_].*$'],
            capture_output=True, cwd=repository.root_path,
        ).returncode
        log.info('Set better Objective-C diffing behavior for this repository!')

        commands_to_color = ('color.status', 'color.diff', 'color.branch')
        need_prompt_color = args.all or any([not local_config.get(command) for command in commands_to_color])
        if args.defaults or (need_prompt_color and Terminal.choose(
            'Auto-color status, diff, and branch for this repository?',
            default='Yes', options=('Yes', 'Skip'),
        ) == 'Yes'):
            for command in commands_to_color:
                if not local_config.get(command):
                    result += run(
                        [local.Git.executable(), 'config', command, 'auto'],
                        capture_output=True, cwd=repository.root_path,
                    ).returncode

        if args.merge is None:
            args.merge = repository.config(location='project')['pull.rebase'] == 'false'
        log.info('Using {} merge strategy for this repository'.format('merge commits as a' if args.merge else 'a rebase'))
        if run(
            [local.Git.executable(), 'config', 'pull.rebase', 'false' if args.merge else 'true'],
            capture_output=True, cwd=repository.root_path,
        ).returncode:
            sys.stderr.write('Failed to use {} as the merge strategy for this repository\n'.format('merge commits' if args.merge else 'rebase'))
            result += 1

        need_prompt_auto_update = args.all or not local_config.get('webkitscmpy.auto-rebase-branch')
        if not args.merge and not args.defaults and need_prompt_auto_update:
            log.info('Setting auto update on PR creation...')
            response = Terminal.choose(
                'Would you like to automatically rebase your branch when creating or\nupdating a pull request?',
                default='Yes', options=('Yes', 'No', 'Later'),
            )
            if response in ['Yes', 'No']:
                if run(
                    [local.Git.executable(), 'config', 'webkitscmpy.auto-rebase-branch', 'true' if response == 'Yes' else 'false'],
                    capture_output=True, cwd=repository.root_path,
                ).returncode:
                    sys.stderr.write('Failed to {} auto update\n'.format('enable' if response == 'Yes' else 'disable'))
                    result += 1
                else:
                    log.info('{} auto update on PR creation'.format('Enabled' if response == 'Yes' else 'Disabled'))
            else:
                log.info('Skipped explicit auto update configuration')

        if args.all or not local_config.get('webkitscmpy.history'):
            if repository.config(location='project')['webkitscmpy.history'] == 'never':
                pr_history = 'never'
            elif repository.config(location='project')['webkitscmpy.pull-request'] != 'overwrite':
                pr_history = None
            elif args.defaults:
                pr_history = repository.config(location='project')['webkitscmpy.history']
            else:
                pr_history = Terminal.choose(
                    'Would you like to create new branches to retain history when you overwrite\na pull request branch?',
                    default=repository.config(location='project')['webkitscmpy.history'],
                    options=repository.PROJECT_CONFIG_OPTIONS['webkitscmpy.history'],
                )
            if pr_history and run(
                [local.Git.executable(), 'config', 'webkitscmpy.history', pr_history],
                capture_output=True, cwd=repository.root_path,
            ).returncode:
                sys.stderr.write("Failed to set '{}' as the default history management approach\n".format(pr_history))
                result += 1

        if hooks:
            for hook in os.listdir(hooks):
                source_path = os.path.join(hooks, hook)
                if not os.path.isfile(source_path):
                    continue
                log.info('Configuring and copying hook {} for this repository'.format(source_path))
                with open(source_path, 'r') as f:
                    from jinja2 import Template
                    contents = Template(f.read()).render(
                        location=source_path,
                        python=os.path.basename(sys.executable),
                        prefer_radar=bool(radar.Tracker.radarclient()),
                    )

                target = os.path.join(repository.common_directory, 'hooks', hook)
                if not os.path.exists(os.path.dirname(target)):
                    os.makedirs(os.path.dirname(target))
                with open(target, 'w') as f:
                    f.write(contents)
                    f.write('\n')
                os.chmod(target, 0o775)

        if args.all or not local_config.get('core.editor'):
            log.info('Setting git editor for {}...'.format(repository.root_path))
            editor_name = 'default' if args.defaults else Terminal.choose(
                'Pick a commit message editor for this repository',
                options=['default'] + [program.name for program in Editor.programs()],
                default='default',
                numbered=True,
            )
            editor = None
            if editor_name == 'default':
                for variable in ['SVN_LOG_EDITOR', 'LOG_EDITOR']:
                    if os.environ.get(variable):
                        log.info("Setting contents of '{}' as editor".format(variable))
                        editor_name = variable
                        editor = os.environ.get(variable)
                        break
            else:
                editor = ' '.join([arg.replace(' ', '\\ ') for arg in Editor.by_name(editor_name).wait])

            if not editor:
                log.info('Using the default git editor for this repository')
            elif run(
                [local.Git.executable(), 'config', 'core.editor', editor],
                capture_output=True,
                cwd=repository.root_path,
            ).returncode:
                sys.stderr.write('Failed to set the git editor to {} for this repository\n'.format(editor_name))
                result += 1
            else:
                log.info("Set git editor to '{}' for this repository".format(editor_name))

        # Check if we need to define a credential helper
        http_remote = local.Git.HTTP_REMOTE.match(repository.url())
        can_push = not http_remote
        rmt = repository.remote()
        if not can_push and rmt and getattr(rmt, 'credentials', None):
            username, password = rmt.credentials()
            if username and password and not run([
                repository.executable(), 'config',
                'credential.{}.helper'.format('/'.join(rmt.url.split('/')[:3])),
                '!f() {{ {} -C {} credentials; }}; f'.format(sys.argv[0], rmt.url),
            ], capture_output=True, cwd=repository.root_path).returncode:
                can_push = True

        # Without a credential helper, pushing to http repositories is difficult, offer to change http checkouts to ssh
        if not can_push and not args.defaults and Terminal.choose(
            "http(s) based remotes will prompt for your password every time when pushing,\nit is recommended to convert to a ssh remote, would you like to convert to a ssh remote?",
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
        available_remotes = []
        if isinstance(rmt, remote.GitHub):
            forking = True
            username, _ = rmt.credentials(required=True, validate=True, save_in_keyring=True)
        else:
            forking = False
            username = getpass.getuser()

        # Check and configure alternate remotes
        project_remotes = {}
        for config_arg, url in repository.config().items():
            if not config_arg.startswith('webkitscmpy.remotes'):
                continue

            for match in [repository.SSH_REMOTE.match(url), repository.HTTP_REMOTE.match(url)]:
                if not match:
                    continue
                project_remotes[config_arg.split('.')[-1]] = [
                    'https://{}/{}.git'.format(match.group('host'), match.group('path')),
                    'http://{}/{}.git'.format(match.group('host'), match.group('path')),
                    'git@{}:{}.git'.format(match.group('host'), match.group('path')),
                    'ssh://{}:{}.git'.format(match.group('host'), match.group('path')),
                ]
                break
            else:
                project_remotes[config_arg.split('.')[-1]] = [url]

        protocol = repository.url().split('://')[0] + '://'
        if '@' in protocol:
            protocol = protocol.split('@')[0] + '@'
        if not project_remotes.get('origin') or repository.url() in project_remotes['origin']:
            for name, urls in project_remotes.items():
                if name == 'origin' or not urls:
                    continue

                # We can check via the remote if the current user has access
                nw_rmt = None
                if urls[0].startswith('http'):
                    try:
                        nw_rmt = remote.Scm.from_url(urls[0][:-4])
                        with OutputCapture():
                            nw_rmt.default_branch
                    except RuntimeError:
                        log.info('{} does not have access to {}'.format(username, urls[0]))
                        continue
                    except OSError:
                        pass

                remote_to_add = urls[0]
                for url in urls:
                    if url.startswith(protocol):
                        remote_to_add = url
                        break
                if cls._add_remote(repository, name, remote_to_add, fetch=True):
                    result += 1
                else:
                    available_remotes.append(name)
                if not isinstance(nw_rmt, remote.GitHub):
                    continue
                if cls.github(args, nw_rmt, remote=name, team=repository.config().get('webkitscmpy.access.{}'.format(name), None)):
                    result += 1
                    continue
                log.info("Adding forked {remote} remote as '{username}-{remote}' and '{remote}-fork'...".format(
                    remote=name, username=username,
                ))
                for fork_name in ['{}-{}'.format(username, name), '{}-fork'.format(name)]:
                    if cls._add_remote(repository, fork_name, cls._fork_remote(repository.url(), username, '{}-{}'.format(rmt.name, name)), fetch=False):
                        result += 1
                    else:
                        available_remotes.append(fork_name)

        else:
            warning = '''Checkout using {actual} as origin remote
Project defines {expected} as its origin remote
Automation may create pull requests and forks in unexpected locations
'''.format(actual=repository.url(), expected=project_remotes['origin'][0])

            if forking:
                forking = Terminal.choose('{}Are you sure you want to set up a fork?\n'.format(warning), default='No')
            else:
                sys.stderr.write(warning)

        if not forking or forking == 'No':
            return result

        if cls.github(args, rmt, team=repository.config().get('webkitscmpy.access.origin', None), **kwargs):
            return result + 1

        log.info("Adding forked remote as '{}' and 'fork'...".format(username))
        for name in [username, 'fork']:
            if cls._add_remote(repository, name, cls._fork_remote(repository.url(), username, rmt.name), fetch=False):
                result += 1
            else:
                available_remotes.append(name)

        for rem in available_remotes:
            if 'fork' not in rem:
                continue
            log.info("Fetching '{}'".format(rem))
            result += run(
                [repository.executable(), 'fetch', rem],
                capture_output=True, cwd=repository.root_path,
            ).returncode
        return result

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            '--defaults', '--no-defaults', action=arguments.NoAction, default=None,
            help='Do not prompt the user for defaults, always use (or do not use) them',
        )
        parser.add_argument(
            '--merge', '--no-merge', action=arguments.NoAction, default=None,
            help='Use a merge-commit workflow instead of a rebase workflow',
        )
        parser.add_argument(
            '--all', '-a', action='store_true', default=False,
            help='Prompt the user for all options, do not assume responses from the current configuration',
        )

    @classmethod
    def main(cls, args, repository, **kwargs):
        import jinja2

        if isinstance(repository, local.Git):
            if 'true' != repository.config().get('webkitscmpy.setup', ''):
                info_url = 'https://github.com/WebKit/WebKit/wiki/Git-Config#Configuration-Options'
                print('For detailed information about the options configured by this script, please see:\n{}'.format(info_url))
                if not args.defaults and Terminal.choose("Would you like to open this URL in your browser?", default='Yes') == 'Yes':
                    if not Terminal.open_url(info_url):
                        sys.stderr.write("Failed to open '{}' in the browser, continuing\n".format(info_url))
                print('\n')

            result = cls.git(args, repository, **kwargs)

            if result:
                print('Setup failed')
            else:
                print('Setup succeeded!')
                run(
                    [local.Git.executable(), 'config', 'webkitscmpy.setup', 'true'],
                    capture_output=True,
                    cwd=repository.root_path,
                )

            return result

        if isinstance(repository, remote.GitHub):
            result = cls.github(args, repository, **kwargs)
            print('Setup failed' if result else 'Setup succeeded!')
            return result

        sys.stderr.write('No setup required for {}\n'.format(
            getattr(repository, 'root_path', getattr(repository, 'url', '?')),
        ))
        return 1

# Copyright (C) 2020, 2021 Apple Inc. All rights reserved.
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
import tempfile
import subprocess
import sys

from webkitcorepy import arguments, run, string_utils
from webkitscmpy import log
from ..command import Command


class Canonicalize(Command):
    name = 'canonicalize'
    help = 'Take the set of commits which have not yet been pushed and edit history to normalize the ' +\
           'committers with existing contributor mapping and add identifiers to commit messages'

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            '--identifier', '--no-identifier',
            help='Add in the identifier to commit messages, true by default',
            action=arguments.NoAction,
            dest='identifier',
            default=True,
        )
        parser.add_argument(
            '--remote',
            help='Compare against a different remote',
            dest='remote',
            default='origin',
        )
        parser.add_argument(
            '--number', '-n',  type=int,
            help='Number of commits to be canonicalized, regardless of the state of the remote',
            dest='number',
            default=None,
        )

    @classmethod
    def main(cls, args, repository, identifier_template=None, **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1
        if not repository.path:
            sys.stderr.write('Cannot canonicalize commits on a remote repository\n')
            return 1
        if not repository.is_git:
            sys.stderr.write('Commits can only be canonicalized on a Git repository\n')
            return 1

        branch = repository.branch
        if not branch:
            sys.stderr.write('Failed to determine current branch\n')
            return -1

        num_commits_to_canonicalize = args.number
        if not num_commits_to_canonicalize:
            result = run([
                repository.executable(), 'rev-list',
                '--count', '--no-merges',
                '{remote}/{branch}..{branch}'.format(remote=args.remote, branch=branch),
            ], capture_output=True, cwd=repository.root_path)
            if result.returncode:
                sys.stderr.write('Failed to find local commits\n')
                return -1
            num_commits_to_canonicalize = int(result.stdout.rstrip())
        if num_commits_to_canonicalize <= 0:
            print('No local commits to be edited')
            return 0
        log.warning('{} to be edited...'.format(string_utils.pluralize(num_commits_to_canonicalize, 'commit')))

        base = repository.find('{}~{}'.format(branch, num_commits_to_canonicalize))
        log.info('Base commit is {} (ref {})'.format(base, base.hash))

        log.debug('Saving contributors to temp file to be picked up by child processes')
        contributors = os.path.join(tempfile.gettempdir(), '{}-contributors.json'.format(os.getpid()))
        try:
            with open(contributors, 'w') as file:
                repository.contributors.save(file)

            message_filter = [
                '--msg-filter',
                "{} {} '{}'".format(
                    sys.executable,
                    os.path.join(os.path.dirname(__file__), 'message.py'),
                    identifier_template or 'Identifier: {}',
                ),
            ] if args.identifier else []

            with open(os.devnull, 'w') as devnull:
                subprocess.check_call([
                    repository.executable(), 'filter-branch', '-f',
                    '--env-filter', '''{overwrite_message}
committerOutput=$({python} {committer_py} {contributor_json})
KEY=''
VALUE=''
for word in $committerOutput; do
    if [[ $word == GIT_* ]] ; then
        if [[ $KEY == GIT_* ]] ; then
            {setting_message}
            printf -v $KEY "${{VALUE::$((${{#VALUE}} - 1))}}"
            KEY=''
            VALUE=''
        fi
    fi
    if [[ "$KEY" == "" ]] ; then
        KEY="$word"
    else
        VALUE="$VALUE$word "
    fi
done
if [[ $KEY == GIT_* ]] ; then
    {setting_message}
    printf -v $KEY "${{VALUE::$((${{#VALUE}} - 1))}}"
fi'''.format(
                        overwrite_message='' if log.level > logging.INFO else 'echo "Overwriting $GIT_COMMIT"',
                        python=sys.executable,
                        committer_py=os.path.join(os.path.dirname(__file__), 'committer.py'),
                        contributor_json=contributors,
                        setting_message='' if log.level > logging.DEBUG else 'echo "    $KEY=$VALUE"',
                    ),
                ] + message_filter + ['{}...{}'.format(branch, base.hash)],
                    cwd=repository.root_path,
                    env={'FILTER_BRANCH_SQUELCH_WARNING': '1', 'PYTHONPATH': ':'.join(sys.path)},
                    stdout=devnull if log.level > logging.WARNING else None,
                    stderr=devnull if log.level > logging.WARNING else None,
                )

        except subprocess.CalledProcessError:
            sys.stderr.write('Failed to modify local commit messages\n')
            return -1

        finally:
            os.remove(contributors)

        print('{} successfully canonicalized!'.format(string_utils.pluralize(num_commits_to_canonicalize, 'commit')))

        return 0

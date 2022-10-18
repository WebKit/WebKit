# Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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

import json
import sys

from .command import Command
from datetime import datetime
from webkitcorepy import arguments
from webkitscmpy import Commit, local


class Info(Command):
    name = 'info'
    help = 'Print information about the HEAD commit'

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            '--json', '-j',
            help='Convert the commit to a machine-readable JSON object',
            action='store_true',
            dest='json',
            default=False,
        )
        parser.add_argument(
            '--log', '--no-log',
            help='Include the commit message for the requested commit',
            action=arguments.NoAction,
            dest='include_log',
            default=True,
        )

    @classmethod
    def print_(cls, args, commits, verbose_default=0):
        if args.json:
            print(json.dumps(commits if len(commits) > 1 else commits[0], cls=Commit.Encoder, indent=4))
            return 0

        if args.verbose < verbose_default:
            for commit in commits:
                print('{identifier} | {hash}{revision}{title}'.format(
                    identifier=commit,
                    hash=commit.hash[:Commit.HASH_LABEL_SIZE] if commit.hash else '',
                    revision='{}r{}'.format(', ' if commit.hash else '', commit.revision) if commit.revision else '',
                    title=' | {}'.format(commit.message.splitlines()[0]) if commit.message else ''
                ))
            return 0

        previous = False
        for commit in commits:
            if previous:
                print('-' * 20)
            previous = True
            if commit.message:
                print(u'Title: {}'.format(commit.message.splitlines()[0]))
            try:
                print(u'Author: {}'.format(commit.author))
            except (UnicodeEncodeError, UnicodeDecodeError):
                print('Error: Unable to  print commit author name, please file a bug if seeing this locally.')
            print(datetime.fromtimestamp(commit.timestamp).strftime('Date: %a %b %d %H:%M:%S %Y'))
            if args.verbose > verbose_default or commit.revision:
                print('Revision: {}'.format(commit.revision or 'N/A'))
            if args.verbose > verbose_default or commit.hash:
                print('Hash: {}'.format(commit.hash[:Commit.HASH_LABEL_SIZE] if commit.hash else 'N/A'))
            print(u'Identifier: {}'.format(commit))

            if args.verbose > verbose_default:
                for line in commit.message.splitlines():
                    print(u'    {}'.format(line))

        return 0

    @classmethod
    def main(cls, args, repository, reference='HEAD', **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1

        scopes = getattr(args, 'scopes', None)

        try:
            if '..' in reference:
                if '...' in reference:
                    sys.stderr.write("'find' sub-command only supports '..' notation\n")
                    return 1
                references = reference.split('..')
                if len(references) > 2:
                    sys.stderr.write('Can only include two references in a range\n')
                    return 1
                kwargs_to_pass = dict(
                    begin=dict(argument=references[0]),
                    end=dict(argument=references[1]),
                )
                if scopes:
                    if not isinstance(repository, local.Git):
                        sys.stderr.write("Can only use the '--scope' argument on a native Git repository\n")
                        return 1
                    kwargs_to_pass['scopes'] = scopes
                commits = [commit for commit in repository.commits(**kwargs_to_pass)]
            else:
                if scopes:
                    sys.stderr.write('Scope argument invalid when only one commit specified\n')
                    return 1
                commits = [repository.find(reference, include_log=args.include_log)]

        except (local.Scm.Exception, TypeError, ValueError) as exception:
            # ValueErrors and Scm exceptions usually contain enough information to be displayed
            # to the user as an error
            sys.stderr.write(str(exception) + '\n')
            return 1

        for commit in commits:
            if args.include_log and args.verbose > 0 and not commit.message:
                sys.stderr.write("Failed to find the commit message for '{}'\n".format(commit))
                return 1

        return cls.print_(args, commits)


class Find(Command):
    name = 'find'
    help = 'Given an identifier, revision or hash, normalize and print the commit'
    aliases = ['list']

    @classmethod
    def parser(cls, parser, loggers=None):
        Info.parser(parser, loggers=loggers)

        parser.add_argument(
            'argument', nargs=1,
            type=str, default=None,
            help='String representation of a commit or branch to be normalized',
        )
        parser.add_argument(
            '--scope', '-s',
            help='Filter queries for commit ranges to specific paths in the repository',
            action='append',
            dest='scopes',
            default=None,
        )

    @classmethod
    def main(cls, args, repository, **kwargs):
        return Info.main(args, repository=repository, reference=args.argument[0], **kwargs)

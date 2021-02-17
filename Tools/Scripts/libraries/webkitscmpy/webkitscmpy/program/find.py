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

import json
import sys

from .command import Command
from datetime import datetime
from webkitcorepy import arguments
from webkitscmpy import Commit, local


class Find(Command):
    name = 'find'
    help = 'Given an identifier, revision or hash, normalize and print the commit'

    @classmethod
    def parser(cls, parser, loggers=None):
        output_args = arguments.LoggingGroup(
            parser,
            loggers=loggers,
            help='{} amount of logging and commit information displayed'
        )
        output_args.add_argument(
            '--json', '-j',
            help='Convert the commit to a machine-readable JSON object',
            action='store_true',
            dest='json',
            default=False,
        )
        output_args.add_argument(
            '--log', '--no-log',
            help='Include the commit message for the requested commit',
            action=arguments.NoAction,
            dest='include_log',
            default=True,
        )

        parser.add_argument(
            'argument', nargs=1,
            type=str, default=None,
            help='String representation of a commit or branch to be normalized',
        )

    @classmethod
    def main(cls, args, repository, **kwargs):
        try:
            commit = repository.find(args.argument[0], include_log=args.include_log)
        except (local.Scm.Exception, ValueError) as exception:
            # ValueErrors and Scm exceptions usually contain enough information to be displayed
            # to the user as an error
            sys.stderr.write(str(exception) + '\n')
            return 1

        if args.verbose > 0 and not commit.message:
            sys.stderr.write("Failed to find the  commit message for '{}'\n".format(commit))
            return 1

        if args.json:
            print(json.dumps(commit, cls=Commit.Encoder, indent=4))
            return 0

        if args.verbose < 0:
            print('{identifier} | {hash}{revision}{title}'.format(
                identifier=commit,
                hash=commit.hash[:Commit.HASH_LABEL_SIZE] if commit.hash else '',
                revision='{}r{}'.format(', ' if commit.hash else '', commit.revision) if commit.revision else '',
                title=' | {}'.format(commit.message.splitlines()[0]) if commit.message else ''
            ))
            return 0

        if commit.message:
            print(u'Title: {}'.format(commit.message.splitlines()[0]))
        print(u'Author: {}'.format(commit.author))
        print(u'Identifier: {}'.format(commit))
        print(datetime.fromtimestamp(commit.timestamp).strftime('Date: %a %b %d %H:%M:%S %Y'))
        if args.verbose > 0 or commit.revision:
            print('Revision: {}'.format(commit.revision or 'N/A'))
        if args.verbose > 0 or commit.hash:
            print('Hash: {}'.format(commit.hash[:Commit.HASH_LABEL_SIZE] if commit.hash else 'N/A'))

        if args.verbose > 0:
            for line in commit.message.splitlines():
                print(u'    {}'.format(line))

        return 0

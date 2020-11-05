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

import argparse
import json
import logging
import os
import sys

from datetime import datetime
from webkitcorepy import arguments, log as webkitcorepy_log
from webkitscmpy import Commit, local, log


class Command(object):
    name = None
    help = None

    @classmethod
    def parser(cls, parser, repository):
        if cls.name is None:
            raise NotImplementedError('Command does not have a name')
        if cls.help is None:
            raise NotImplementedError("'{}' does not have a help message")

    @classmethod
    def main(cls, repository):
        sys.stderr.write('No command specified\n')
        return -1


class Find(Command):
    name = 'find'
    help = 'Given an identifier, revision or hash, normalize and print the commit'

    @classmethod
    def parser(cls, parser, repository, loggers=None):
        arguments.LoggingGroup(
            parser,
            loggers=loggers,
            help='{} amount of logging and commit information displayed'
        ).add_argument(
            '--json', '-j',
            help='Convert the commit to a machine-readable JSON object',
            action='store_true',
            dest='json',
            default=False,
        )

        parser.add_argument(
            'argument', nargs=1,
            type=str, default=None,
            help='String representation of a commit or branch to be normalized',
        )

    @classmethod
    def main(cls, args, repository):
        try:
            commit = repository.find(args.argument[0])
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
            print('Title: {}'.format(commit.message.splitlines()[0]))
        print('Author: {}'.format(commit.author))
        print('Identifier: {}'.format(commit))
        print(datetime.fromtimestamp(commit.timestamp).strftime('Date: %a %b %d %H:%M:%S %Y'))
        if args.verbose > 0 or commit.revision:
            print('Revision: {}'.format(commit.revision or 'N/A'))
        if args.verbose > 0 or commit.hash:
            print('Hash: {}'.format(commit.hash[:Commit.HASH_LABEL_SIZE] if commit.hash else 'N/A'))

        if args.verbose > 0:
            for line in commit.message.splitlines():
                print('    {}'.format(line))

        return 0


class Checkout(Command):
    name = 'checkout'
    help = 'Given an identifier, revision or hash, normalize and checkout that commit'

    @classmethod
    def parser(cls, parser, repository, loggers=None):
        arguments.LoggingGroup(
            parser,
            loggers=loggers,
            help='{} amount of logging and commit information displayed'
        )

        parser.add_argument(
            'argument', nargs=1,
            type=str, default=None,
            help='String representation of a commit or branch to be normalized',
        )

    @classmethod
    def main(cls, args, repository):
        try:
            commit = repository.checkout(args.argument[0])
        except (local.Scm.Exception, ValueError) as exception:
            # ValueErrors and Scm exceptions usually contain enough information to be displayed
            # to the user as an error
            sys.stderr.write(str(exception) + '\n')
            return 1

        if not commit:
            sys.stderr.write("Failed to map '{}'\n".format(args.argument[0]))
            return 1
        return 0


def main(args=None, path=None, loggers=None):
    logging.basicConfig(level=logging.WARNING)

    loggers = [logging.getLogger(), webkitcorepy_log,  log] + (loggers or [])

    repository = local.Scm.from_path(path=path or os.getcwd())

    parser = argparse.ArgumentParser(
        description='Custom git tooling from the WebKit team to interact with a ' +
                    'repository using identifers',
    )
    arguments.LoggingGroup(parser)
    subparsers = parser.add_subparsers(help='sub-command help')

    for program in [Find, Checkout]:
        subparser = subparsers.add_parser(program.name, help=program.help)
        subparser.set_defaults(main=program.main)
        program.parser(subparser, repository=repository, loggers=loggers)

    parsed = parser.parse_args(args=args)

    return parsed.main(args=parsed, repository=repository)

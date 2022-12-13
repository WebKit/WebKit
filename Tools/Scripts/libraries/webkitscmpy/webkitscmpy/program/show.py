#!/usr/bin/env python3

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

import sys

from webkitcorepy import arguments, Terminal
from webkitscmpy import local
from webkitscmpy.program.command import FilteredCommand


class Show(FilteredCommand):
    name = 'show'
    help = "Filter raw output of 'git show' to replace native commit representation with identifiers"

    @classmethod
    def parser(cls, parser, loggers=None):
        FilteredCommand.parser(parser, loggers=loggers)
        parser.add_argument(
            '--pretty', '--format', type=str,
            help='Pretty-print the contents of the commit logs in a given format, where <format> can be\n'
                'one of oneline, short, medium, full, fuller, reference, email, raw, format:<string> and\n'
                'tformat:<string>. When <format> is none of the above, and has %%placeholder in it, it\n'
                'acts as if --pretty=tformat:<format> were given.\n\n'
                'See the "PRETTY FORMATS" section for some additional details for each format. When\n'
                '=<format> part is omitted, it defaults to medium.\n\n'
                'Note: you can specify the default pretty format in the repository configuration (see\n'
                'git-config(1)).',
            dest='pretty',
            default=None,
        )
        parser.add_argument(
            '--abbrev-commit', '--no-abbrev-commit',
            help='Instead of showing the full 40-byte hexadecimal commit object name, show a prefix that\n'
                'names the object uniquely. "--abbrev=<n>" (which also modifies diff output, if it is\n'
                'displayed) option can be used to specify the minimum length of the prefix.',
            action=arguments.NoAction,
            dest='abbrev_commit',
            default=None,
        )
        parser.add_argument(
            '--oneline',
            help='This is a shorthand for "--pretty=oneline --abbrev-commit" used together.',
            action='store_true',
            dest='oneline',
            default=None,
        )

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not isinstance(repository, local.Git):
            sys.stderr.write("Can only 'show' on a native Git repository\n")
            return 1

        config = getattr(repository, 'config', lambda: {})()
        Terminal.colors = config.get('color.diff', config.get('color.ui', 'auto')) != 'false'

        pretty = getattr(args, 'pretty', None)
        if pretty:
            args.args.insert(0, '--pretty={}'.format(pretty))
        abbrev_commit = getattr(args, 'abbrev_commit', None)
        if abbrev_commit is not None:
            args.args.insert(0, '--abbrev-commit' if abbrev_commit else '--no-abbrev-commit')
        oneline = getattr(args, 'oneline', None)
        if oneline is not None:
            args.args.insert(0, '--oneline')

        return cls.pager(args, repository, file=__file__, **kwargs)


if __name__ == '__main__':
    sys.exit(Show.main(
        sys.argv[4:],
        local.Scm.from_path(path=sys.argv[1], cached=True),
        representation=sys.argv[2],
        isatty=sys.argv[3] == 'isatty',
    ))

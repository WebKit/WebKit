#!/usr/bin/env python3

# Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
from webkitscmpy.program.show import Show


class Log(FilteredCommand):
    name = 'log'
    help = "Filter raw output of 'git log' or 'svn log' to replace native commit representation with identifiers"

    @classmethod
    def parser(cls, parser, loggers=None):
        Show.parser(parser, loggers=loggers)
        parser.add_argument(
            '--max-count', '-n', type=int,
            help='Limit the number of commits to output.',
            dest='max_count',
            default=None,
        )
        parser.add_argument(
            '--skip', type=int,
            help='Skip number commits before starting to show the commit output.',
            dest='skip',
            default=None,
        )

    @classmethod
    def main(cls, args, repository, **kwargs):
        config = getattr(repository, 'config', lambda: {})()
        Terminal.colors = config.get('color.diff', config.get('color.ui', 'auto')) != 'false'

        max_count = getattr(args, 'max_count', None)
        if max_count:
            args.args.insert(0, '--max-count={}'.format(max_count))
        skip = getattr(args, 'skip', None)
        if skip:
            args.args.insert(0, '--skip={}'.format(skip))
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
    sys.exit(Log.main(
        sys.argv[4:],
        local.Scm.from_path(path=sys.argv[1], cached=True),
        representation=sys.argv[2],
        isatty=sys.argv[3] == 'isatty',
    ))

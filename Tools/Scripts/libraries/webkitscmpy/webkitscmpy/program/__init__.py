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

import argparse
import logging
import os

from webkitcorepy import arguments, log as webkitcorepy_log
from webkitscmpy import local, log, remote

from .canonicalize import Canonicalize
from .clean import Clean
from .command import Command
from .checkout import Checkout
from .find import Find
from .pull import Pull
from .setup_git_svn import SetupGitSvn


def main(args=None, path=None, loggers=None, contributors=None, identifier_template=None, subversion=None):
    logging.basicConfig(level=logging.WARNING)

    loggers = [logging.getLogger(), webkitcorepy_log,  log] + (loggers or [])

    parser = argparse.ArgumentParser(
        description='Custom git tooling from the WebKit team to interact with a ' +
                    'repository using identifers',
    )
    arguments.LoggingGroup(parser)

    group = parser.add_argument_group('Repository')
    group.add_argument(
        '--path', '-p', '-C',
        dest='repository', default=path or os.getcwd(),
        help='Set the repository path or URL to be used',
        action='store',
    )

    subparsers = parser.add_subparsers(help='sub-command help')

    programs = [Find, Checkout, Canonicalize, Pull, Clean]
    if subversion:
        programs.append(SetupGitSvn)

    for program in programs:
        subparser = subparsers.add_parser(program.name, help=program.help)
        subparser.set_defaults(main=program.main)
        program.parser(subparser, loggers=loggers)

    parsed = parser.parse_args(args=args)

    if parsed.repository.startswith(('https://', 'http://')):
        repository = remote.Scm.from_url(parsed.repository, contributors=contributors)
    else:
        repository = local.Scm.from_path(path=parsed.repository, contributors=contributors)

    return parsed.main(
        args=parsed,
        repository=repository,
        identifier_template=identifier_template,
        subversion=subversion,
    )

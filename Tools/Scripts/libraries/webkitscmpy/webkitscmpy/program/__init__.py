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
import sys

from .blame import Blame
from .branch import Branch
from .canonicalize import Canonicalize
from .clean import Clean
from .command import Command
from .checkout import Checkout
from .find import Find, Info
from .log import Log
from .pull import Pull
from .pull_request import PullRequest
from .setup_git_svn import SetupGitSvn
from .setup import Setup

from webkitcorepy import arguments, log as webkitcorepy_log
from webkitscmpy import local, log, remote


def main(
    args=None, path=None, loggers=None, contributors=None,
    identifier_template=None, subversion=None, additional_setup=None, hooks=None,
):
    logging.basicConfig(level=logging.WARNING)

    loggers = [logging.getLogger(), webkitcorepy_log,  log] + (loggers or [])

    parser = argparse.ArgumentParser(
        description='Custom git tooling from the WebKit team to interact with a ' +
                    'repository using identifers',
    )
    arguments.LoggingGroup(
        parser,
        loggers=loggers,
        help='{} amount of logging and commit information displayed',
    )

    group = parser.add_argument_group('Repository')
    group.add_argument(
        '--path', '-p', '-C',
        dest='repository', default=path or os.getcwd(),
        help='Set the repository path or URL to be used',
        action='store',
    )

    subparsers = parser.add_subparsers(help='sub-command help')
    programs = [Blame, Branch, Canonicalize, Checkout, Clean, Find, Info, Log, Pull, PullRequest, Setup]
    if subversion:
        programs.append(SetupGitSvn)

    for program in programs:
        kwargs = dict(help=program.help)
        if sys.version_info > (3, 0):
            kwargs['aliases'] = program.aliases
        subparser = subparsers.add_parser(program.name, **kwargs)
        subparser.set_defaults(main=program.main)
        subparser.set_defaults(program=program.name)
        subparser.set_defaults(aliases=program.aliases)
        arguments.LoggingGroup(
            subparser,
            loggers=loggers,
            help='{} amount of logging and commit information displayed',
        )
        program.parser(subparser, loggers=loggers)

    args = args or sys.argv[1:]
    parsed, unknown = parser.parse_known_args(args=args)
    if unknown:
        program_index = 0
        for candidate in [parsed.program] + parsed.aliases:
            if candidate in args:
                program_index = args.index(candidate)
                break
        if getattr(parsed, 'args', None):
            parsed.args = [arg for arg in args[program_index:] if arg in parsed.args or arg in unknown]
        if any([option not in getattr(parsed, 'args', []) for option in unknown]):
            parsed = parser.parse_args(args=args)

    if parsed.repository.startswith(('https://', 'http://')):
        repository = remote.Scm.from_url(parsed.repository, contributors=None if callable(contributors) else contributors)
    else:
        repository = local.Scm.from_path(path=parsed.repository, contributors=None if callable(contributors) else contributors)

    if callable(contributors):
        repository.contributors = contributors(repository) or repository.contributors
    if callable(identifier_template):
        identifier_template = identifier_template(repository)
    if callable(subversion):
        subversion = subversion(repository)
    if callable(hooks):
        hooks = hooks(repository)

    if sys.version_info > (3, 0):
        import inspect
    else:
        import inspect2 as inspect
    if callable(additional_setup) and list(inspect.signature(additional_setup).parameters.keys()) == ['repository']:
        additional_setup = additional_setup(repository)

    if not getattr(parsed, 'main', None):
        parser.print_help()
        return -1

    return parsed.main(
        args=parsed,
        repository=repository,
        identifier_template=identifier_template,
        subversion=subversion,
        additional_setup=additional_setup,
        hooks=hooks,
    )

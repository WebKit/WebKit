# Copyright (C) 2020-2024 Apple Inc. All rights reserved.
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

from __future__ import annotations

import argparse
import logging
import os
import re
import sys
import traceback
from typing import TYPE_CHECKING, Any, Callable, Optional, Sequence, TypeVar, Union

from .apply import Apply
from .blame import Blame
from .branch import Branch
from .canonicalize import Canonicalize, IdentifierTrailer
from .cherry_pick import CherryPick
from .clean import Clean, DeletePRBranches
from .clone import Clone
from .command import Command
from .commit import Commit
from .conflict import Conflict
from .create_bug import CreateBug
from .diff import Diff
from .squash import Squash
from .checkout import Checkout
from .classify import Classify
from .credentials import Credentials
from .find import Find, Info
from .pickable import Pickable
from .publish import Publish
from .install_git_lfs import InstallGitLFS
from .install_hooks import InstallHooks
from .land import Land
from .log import Log
from .pull import Pull
from .pull_request import PullRequest
from .revert import Revert
from .review import Review
from .setup_git_svn import SetupGitSvn
from .setup import Setup
from .show import Show
from .trace import Trace
from .track import Track
from .tracker_metadata import TrackerMetadata

from webkitbugspy import log as webkitbugspy_log
from webkitcorepy import arguments, filtered_call, log as webkitcorepy_log, Terminal
from webkitscmpy import local, log, remote

if TYPE_CHECKING:
    from logging import Logger, RootLogger
    from webkitscmpy import CommitClassifier, Contributor, ScmBase

    # Most of main()'s settings can also be a function of the repository (or of None, before a repository
    # has been found) which returns the setting.
    T = TypeVar('T')
    PerRepository = Union[T, Callable[[Optional[ScmBase]], Optional[T]]]


def main(
    args: Sequence[str] | None = None, path: str | None = None, loggers: list[RootLogger | Logger] | None = None,
    contributors: PerRepository[Contributor.Mapping] | None = None,
    identifier_template: PerRepository[IdentifierTrailer | str] | None = None, subversion: PerRepository[str] | None = None,
    additional_setup: Callable[..., Any] | None = None, hooks: PerRepository[str] | None = None,
    canonical_svn: PerRepository[bool] | None = None, programs: list[type[Command]] | None = None,
    classifier: PerRepository[CommitClassifier] | None = None, fallback_path: str | None = None, **kwargs: Any
) -> int:
    logging.basicConfig(level=logging.WARNING)

    loggers = [logging.getLogger(), webkitcorepy_log,  webkitbugspy_log, log] + (loggers or [])

    parser = argparse.ArgumentParser(
        description='Custom git tooling from the WebKit team to interact with a ' +
                    'repository using identifiers',
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
    subparser = subparsers.add_parser('help', help='Print all help messages')
    arguments.LoggingGroup(subparser, loggers=loggers)
    subparser.set_defaults(main=lambda *args, **kwargs: parser.print_help())

    programs = [
        Apply, Blame, Branch, Canonicalize, Checkout,
        Clean, Clone, Conflict, CreateBug, Diff, Find, Info, Land, Log, Pull,
        PullRequest, Revert, Review, Setup, InstallGitLFS,
        Credentials, Commit, DeletePRBranches, Squash,
        Pickable, CherryPick, Trace, Track, TrackerMetadata, Show, Publish,
        Classify, InstallHooks,
    ] + (programs or [])
    if subversion:
        programs.append(SetupGitSvn)

    provisional_classifier = classifier(None) if callable(classifier) else classifier
    for program in programs:
        assert program.name is not None  # Only Command itself has no name
        help: str | None
        if callable(program.help):
            help = filtered_call(program.help, classifier=provisional_classifier)
        else:
            help = program.help
        subparser = subparsers.add_parser(
            program.name, help=help, aliases=program.aliases
        )
        subparser.set_defaults(main=program.main)
        subparser.set_defaults(program=program.name)
        subparser.set_defaults(aliases=program.aliases)
        arguments.LoggingGroup(
            subparser,
            loggers=loggers,
            help='{} amount of logging and commit information displayed',
        )
        filtered_call(
            program.parser, subparser,
            classifier=provisional_classifier,
            loggers=loggers,
        )

    args = args or sys.argv[1:]
    parsed, unknown = parser.parse_known_args(args=args)
    if not getattr(parsed, 'program', None):
        parser.print_help()
        return 255
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

    repository: ScmBase | None
    if parsed.repository.startswith(('https://', 'http://')):
        repository = remote.Scm.from_url(
            parsed.repository,
            contributors=None if callable(contributors) else contributors,
            classifier=None if callable(classifier) else classifier,
        )
    else:
        try:
            repository = local.Scm.from_path(
                path=parsed.repository,
                contributors=None if callable(contributors) else contributors,
                classifier=None if callable(classifier) else classifier,
            )
        except OSError:
            log.warning("No repository found at '{}'".format(parsed.repository))
            repository = None

    if repository and callable(contributors):
        repository.contributors = contributors(repository) or repository.contributors
    if repository and callable(classifier):
        repository.classifier = classifier(repository) or repository.classifier
    if callable(identifier_template):
        identifier_template = identifier_template(repository) if repository else None
    if isinstance(identifier_template, str):
        identifier_template = IdentifierTrailer.from_template(identifier_template)
    if callable(subversion):
        subversion = subversion(repository) if repository else None
    if callable(hooks):
        hooks = hooks(repository) if repository else None

    if callable(additional_setup):
        additional_setup = filtered_call(additional_setup, repository=repository)

    if callable(canonical_svn):
        canonical_svn = canonical_svn(repository) if repository else None

    if not getattr(parsed, 'main', None):
        parser.print_help()
        return -1

    # Bugzilla's REST API embeds credentials as plain-text query parameters (login= and
    # password=), which means they can appear in exception tracebacks when requests fail.
    # Scrub them here at the top level so they're never printed to the terminal.
    # This can be removed once Bugzilla auth no longer uses credentials in URLs.
    with Terminal.disable_keyboard_interrupt_stacktracktrace():
        try:
            result: int = parsed.main(
                args=parsed,
                repository=repository,
                identifier_template=identifier_template,
                subversion=subversion,
                additional_setup=additional_setup,
                hooks=hooks,
                canonical_svn=canonical_svn,
                fallback_path=fallback_path,
            )
            return result
        except Exception:
            sys.stderr.write(re.sub(r'(login|password)=[^&]+', r'\1=<REDACTED>', traceback.format_exc()))
            return -1

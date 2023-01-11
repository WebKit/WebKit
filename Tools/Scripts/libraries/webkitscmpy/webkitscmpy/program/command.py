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

import os
import platform
import re
import subprocess
import sys
import time

from webkitcorepy import Terminal
from webkitscmpy.commit import Commit


class Command(object):
    name = None
    aliases = []
    help = None

    @classmethod
    def parser(cls, parser, loggers=None):
        if cls.name is None:
            raise NotImplementedError('Command does not have a name')
        if cls.help is None:
            raise NotImplementedError("'{}' does not have a help message")

    @classmethod
    def main(cls, args, repository, **kwargs):
        sys.stderr.write('No command specified\n')
        return -1


class FilteredCommand(Command):
    IDENTIFIER = 'identifier'
    HASH = 'hash'
    REVISION = 'revision'

    GIT_HEADER_RE = re.compile(r'^(commit )?(?P<hash>[a-f0-9A-F]+)')
    SVN_HEADER_RE = re.compile(r'^(?P<revision>r/d+) | ')

    REVISION_RES = (re.compile(r'^(?P<revision>\d+)\s'), re.compile(r'(?P<revision>[rR]\d+)'))
    HASH_RES = (re.compile(r'^(?P<hash>[a-f0-9A-F]{8}[a-f0-9A-F]+)\s'), re.compile(r'(?P<hash>[a-f0-9A-F]{8}[a-f0-9A-F]+)'))
    IDENTIFIER_RES = (re.compile(r'^(?P<identifier>(\d+\.)?\d+@\S*)'), re.compile(r'(?P<identifier>(\d+\.)?\d+@\S*)'))
    NO_FILTER_RES = [re.compile(r'    Canonical link:'), re.compile(r'    git-svn-id:')]
    DIFF_RE = re.compile(r'^diff --git ')

    REPLACE_MODE = 0
    APPEND_MODE = 1
    HEADER_MODE = 2

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'args', nargs='*',
            type=str, default=None,
            help='Arguments to be passed to the native source-code management tool',
        )
        parser.add_argument(
            '--identifier', '-i',
            help='Represent commits as identifiers',
            dest='representation',
            action='store_const', const=cls.IDENTIFIER,
            default=cls.IDENTIFIER,
        )
        parser.add_argument(
            '--hash',
            help='Represent commits as hashes',
            dest='representation',
            action='store_const', const=cls.HASH,
            default=cls.IDENTIFIER,
        )
        parser.add_argument(
            '--revision', '-r',
            help='Represent commits as revisions',
            dest='representation',
            action='store_const', const=cls.REVISION,
            default=cls.IDENTIFIER,
        )
        parser.add_argument(
            '--representation', type=str,
            help='Change method used to represent a commit (identifier/hash/revision)',
            dest='representation',
            default=cls.IDENTIFIER,
        )

    @classmethod
    def pager(cls, args, repository, file=None, **kwargs):
        from whichcraft import which

        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1
        if not repository.path:
            sys.stderr.write("Cannot run '{}' on remote repository\n".format(cls.name))
            return 1

        if not getattr(repository, 'cache', None) and getattr(repository, 'Cache', None):
            repository.cache = repository.Cache(repository)

        # If we're a terminal, rely on 'more' to display output
        if Terminal.isatty(sys.stdin) and not isinstance(args, list) and file:
            environ = os.environ
            environ['PYTHONPATH'] = ':'.join(sys.path)

            child = subprocess.Popen(
                [sys.executable, file, repository.root_path, args.representation, 'isatty' if Terminal.isatty(sys.stdout) else 'notatty'] + args.args,
                env=environ,
                cwd=os.getcwd(),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            more = subprocess.Popen([which('more')] + (['-F', '-R'] if platform.system() == 'Darwin' else []), stdin=child.stdout)

            try:
                while more.poll() is None and not child.poll():
                    time.sleep(0.25)
            finally:
                if child.returncode is None:
                    child.kill()
                if more.returncode is None:
                    more.kill()
                child_error = child.stderr.read()
                if child_error:
                    (sys.stderr.buffer if sys.version_info > (3, 0) else sys.stderr).write(b'\n' + child_error)
                return child.returncode

        with Terminal.override_atty(sys.stdout, isatty=kwargs.get('isatty')), Terminal.override_atty(sys.stderr, isatty=kwargs.get('isatty')):
            return FilteredCommand.main(args, repository, command=cls.name, **kwargs)

    @classmethod
    def replace(cls, arg, repository):
        parsed = Commit.parse(arg, do_assert=False)
        if not parsed:
            return None
        replacement = None
        if repository.is_svn:
            replacement = repository.cache.to_revision(hash=parsed.hash, identifier=str(parsed) if parsed.identifier else None)
        if repository.is_git:
            replacement = repository.cache.to_hash(revision=parsed.revision, identifier=str(parsed) if parsed.identifier else None)
        return replacement

    @classmethod
    def main(cls, args, repository, command=None, representation=None, **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1
        if not repository.path:
            sys.stderr.write("Cannot run '{}' on remote repository\n".format(command))
            return 1

        cache = getattr(repository, 'cache', None)
        if not cache:
            sys.stderr.write('No cache available, cannot performantly map commit references\n')
            return 1

        if not isinstance(args, list):
            representation = representation or args.representation
            args = args.args
        else:
            representation = representation or cls.IDENTIFIER

        if representation not in (cls.IDENTIFIER, cls.HASH, cls.REVISION):
            sys.stderr.write("'{}' is not a valid commit representation\n".format(representation))
            return 1

        for index in range(len(args)):
            replacement = cls.replace(args[index], repository)
            if replacement:
                args[index] = replacement
                continue
            split = args[index].split('...')
            if len(split) > 1:
                args[index] = '...'.join([
                    cls.replace(component, repository) or component for component in split
                ])
                continue

            for candidate in [
                os.path.abspath(os.path.join(os.getcwd(), args[index])),
                os.path.abspath(os.path.join(repository.root_path, args[index])),
            ]:
                if not candidate.startswith(repository.root_path):
                    continue
                if os.path.exists(candidate):
                    args[index] = candidate
                    break

        log_output = subprocess.Popen(
            [repository.executable(), command] + args,
            cwd=repository.root_path,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            **(dict(encoding='utf-8') if sys.version_info > (3, 0) else dict())
        )
        log_output.poll()

        def replace_line(match, mode=cls.APPEND_MODE, **kwargs):
            reference = kwargs.get(representation)
            if not reference:
                reference = getattr(cache, 'to_{}'.format(representation), lambda **kwargs: None)(**kwargs)

            if reference:
                if isinstance(reference, int):
                    reference = 'r{}'.format(reference)
                if representation == 'hash':
                    reference = reference[:Commit.HASH_LABEL_SIZE]
                if mode == cls.APPEND_MODE:
                    reference = '{} ({})'.format(match.groups()[-1], reference)
                if mode == cls.HEADER_MODE:
                    alternates = [] if match.groups()[-1].startswith(reference) else [match.groups()[-1]]
                    for repr in {'revision', 'hash', 'identifier'} - {'hash' if repository.is_git else 'revision', representation}:
                        if repr in kwargs:
                            continue
                        other = getattr(cache, 'to_{}'.format(repr), lambda **kwargs: None)(**kwargs)
                        if not other:
                            continue
                        if other == 'hash':
                            other = other[:Commit.HASH_LABEL_SIZE]
                        alternates.append('r{}'.format(other) if isinstance(other, int) else other)
                    reference = '{} ({})'.format(reference, ', '.join(alternates))
                return match.group(0).replace(match.groups()[-1], reference)
            return match.group(0)

        res = {}
        if representation != cls.HASH:
            res[cls.HASH] = cls.HASH_RES
        if representation != cls.REVISION:
            res[cls.REVISION] = cls.REVISION_RES
        if representation != cls.IDENTIFIER:
            res[cls.IDENTIFIER] = cls.IDENTIFIER_RES

        header_re = cls.GIT_HEADER_RE if repository.is_git else cls.SVN_HEADER_RE

        try:
            line = log_output.stdout.readline()
            while line:
                if cls.DIFF_RE.match(line):
                    break
                header = header_re.sub(
                    lambda match: replace_line(match, mode=cls.HEADER_MODE, **{'hash' if repository.is_git else 'revision': match.groups()[-1]}),
                    line,
                )
                if header != line:
                    index = 2 if header.startswith('commit') else 1
                    header = header.split(' ')
                    with Terminal.Style(color=Terminal.Text.yellow, style=Terminal.Text.bold).apply(sys.stdout):
                        sys.stdout.write(' '.join(header[:index]))

                    if index < len(header):
                        sys.stdout.write(' ')
                    in_red = index
                    while in_red < len(header):
                        if len(header[in_red]) < 2:
                            break
                        if header[in_red][-1] == ',':
                            in_red += 1
                            continue
                        if header[in_red][-1] == ')' or header[in_red][-2] == ')':
                            in_red += 1
                        break
                    with Terminal.Style(color=Terminal.Text.red).apply(sys.stdout):
                        sys.stdout.write(' '.join(header[index:in_red]))

                    if in_red < len(header):
                        sys.stdout.write(' ')
                    sys.stdout.write(' '.join(header[in_red:]))

                    line = log_output.stdout.readline()
                    continue

                for repr, regexs in res.items():
                    line = regexs[0].sub(
                        lambda match: replace_line(match, mode=cls.REPLACE_MODE, **{repr: match.group()[-1]}),
                        line,
                    )

                if not any(r.match(line) for r in cls.NO_FILTER_RES):
                    for repr, regexs in res.items():
                        line = regexs[1].sub(
                            lambda match: replace_line(match, mode=cls.APPEND_MODE, **{repr: match.group(1)}),
                            line,
                        )

                sys.stdout.write(line)

                line = log_output.stdout.readline()

            while line:
                color = None
                if line.startswith('+') and not line.startswith('+++'):
                    color = Terminal.Text.green
                elif line.startswith('-') and not line.startswith('---'):
                    color = Terminal.Text.red

                if color:
                    with Terminal.Style(color=color).apply(sys.stdout):
                        sys.stdout.write(line)
                else:
                    sys.stdout.write(line)
                line = log_output.stdout.readline()

        finally:
            if not log_output.returncode:
                with Terminal.Style(color=Terminal.Text.red).apply(sys.stderr):
                    sys.stderr.write(log_output.stderr.read())
            if log_output.returncode is None:
                log_output.kill()
        return log_output.returncode

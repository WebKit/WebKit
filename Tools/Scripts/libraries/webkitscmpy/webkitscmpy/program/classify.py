# Copyright (C) 2023 Apple Inc. All rights reserved.
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

from .command import Command
from webkitcorepy import string_utils
from webkitscmpy import ScmBase


class Classify(Command):
    name = 'classify'

    @classmethod
    def help(cls, classifier=None):
        result = 'Repositories may classify different commits so that automation behaves differently '
        result += 'for different types of changes. For example, a commit to garden test expectations '
        result += 'may have less stringent code-review requirements. '

        if classifier and classifier.classes:
            result += 'This repository support the classifications {}. '.format(
                string_utils.join(["'{}'".format(klass.name) for klass in classifier.classes]),
            )
        result += 'Print the classification of a given commit.'
        return result

    @classmethod
    def parser(cls, parser, loggers=None):
        parser.add_argument(
            'argument', nargs='?',
            type=str, default=None,
            help='String representation of a commit to classify',
        )
        parser.add_argument(
            '--list', '-l',
            help='List all commit classifications supported in this repository.',
            action='store_true',
            dest='list',
            default=False,
        )

    @classmethod
    def main(cls, args, repository, **kwargs):
        if not repository:
            sys.stderr.write('No repository provided\n')
            return 1

        if not repository.classifier:
            sys.stderr.write('Provided repository has no classifier specified\n')
            return 255
        if not repository.classifier.classes:
            sys.stderr.write('Repository does not specify any commit classifications\n')
            return 255

        if args.list:
            for klass in repository.classifier.classes:
                print(klass.name)
            return 0

        if not args.argument:
            args.argument = 'HEAD'

        try:
            commit = repository.find(args.argument, include_log=True)
        except (ScmBase.Exception, TypeError, ValueError) as exception:
            # ValueErrors and Scm exceptions usually contain enough information to be displayed
            # to the user as an error
            sys.stderr.write(str(exception) + '\n')
            return 1

        klass = repository.classifier.classify(commit, repository)
        if not klass:
            sys.stderr.write('Provided commit does not match a known class in this repository\n')
            print('None')
            return 1
        print(klass.name)
        return 0

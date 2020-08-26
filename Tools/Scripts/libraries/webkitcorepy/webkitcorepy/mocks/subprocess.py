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

import re
import sys

from functools import cmp_to_key
from webkitcorepy import string_utils, unicode
from webkitcorepy.mocks import ContextStack


class ProcessCompletion(object):
    def __init__(self, returncode=None, stdout=None, stderr=None, elapsed=0):
        self.returncode = 1 if returncode is None else returncode
        self.stdout = string_utils.encode(stdout) if stdout else b''
        self.stderr = string_utils.encode(stderr) if stderr else b''
        self.elapsed = elapsed


class Subprocess(ContextStack):
    """
    Organize ProcessCompletions so calls to subprocess functions will return a ProcessCompletion for
    a set of arguments or trigger a ProcessCompletion generator. mocks.Subprocess makes an attempt to
    prioritize CommandRoute objects for a given set of arguments such that the most specific applicable route
    is prefered.

    Example usage mocking a single command:
        with mocks.Subprocess(
            'ls', completion=mocks.ProcessCompletion(returncode=0, stdout='file1.txt\nfile2.txt\n'),
        ):
            result = run(['ls'], capture_output=True, encoding='utf-8')
            assert result.returncode == 0
            assert result.stdout == 'file1.txt\nfile2.txt\n'

    Example usage mocking a set of commands:
        with mocks.Subprocess(
            mocks.Subprocess.CommandRoute('command-a', 'argument', completion=mocks.ProcessCompletion(returncode=0)),
            mocks.Subprocess.CommandRoute('command-b', completion=mocks.ProcessCompletion(returncode=-1)),
        ):
            result = run(['command-a', 'argument'])
            assert result.returncode == 0

            result = run(['command-b'])
            assert result.returncode == -1
    """
    top = None

    class CommandRoute(object):
        def __init__(self, *args, **kwargs):
            completion = kwargs.pop('completion', ProcessCompletion())
            cwd = kwargs.pop('cwd', None)
            input = kwargs.pop('input', None)
            generator = kwargs.pop('generator', None)
            if kwargs.keys():
                raise TypeError('__init__() got an unexpected keyword argument {}'.format(kwargs.keys()[0]))

            if isinstance(args, str) or isinstance(args, unicode):
                self.args = [args]
            elif not args:
                raise ValueError('Arguments must be provided to a CommandRoute')
            else:
                self.args = args

            self.generator = generator or (lambda *args, **kwargs: completion)
            self.cwd = cwd
            self.input = string_utils.encode(input) if input else None

        def matches(self, *args, **kwargs):
            cwd = kwargs.pop('cwd', None)
            input = kwargs.pop('input', None)
            if kwargs.keys():
                raise TypeError('matches() got an unexpected keyword argument {}'.format(kwargs.keys()[0]))

            if len(self.args) > len(args):
                return False

            for count in range(len(self.args)):
                if self.args[count] is None:
                    return False

                if self.args[count] == args[count]:
                    continue
                elif hasattr(self.args[count], 'match') and self.args[count].match(args[count]):
                    continue
                elif re.match(self.args[count], args[count]):
                    continue
                return False

            if self.cwd is not None and cwd != self.cwd:
                return False
            if self.input is not None and input != self.input:
                return False
            return True

        def __call__(self, *args, **kwargs):
            cwd = kwargs.pop('cwd', None)
            input = kwargs.pop('input', None)
            if kwargs.keys():
                raise TypeError('__call__() got an unexpected keyword argument {}'.format(kwargs.keys()[0]))
            return self.generator(*args, cwd=cwd, input=input)

        @classmethod
        def compare(cls, a, b):
            for candidate in [
                len(b.args) - len(a.args),
                0 if type(a.cwd) == type(b.cwd) else -1 if a.cwd else 1,
                0 if type(a.input) == type(b.input) else -1 if a.input else 1,
            ]:
                if candidate:
                    return candidate
            return 0

    Route = CommandRoute

    @classmethod
    def completion_generator_for(cls, program):
        current = cls.top
        candidates = []
        while current:
            for completion in current.completions:
                if completion.args[0] == program:
                    candidates.append(completion)
                if current.ordered:
                    break
            current = current.previous

        if candidates:
            return candidates

        if sys.version_info > (3, 0):
            raise FileNotFoundError("No such file or directory: '{path}': '{path}'".format(path=program))
        raise OSError('[Errno 2] No such file or directory')

    @classmethod
    def completion_for(cls, *args, **kwargs):
        candidates = [
            candidate for candidate in cls.completion_generator_for(args[0]) if candidate.matches(*args, **kwargs)
        ]
        if not candidates:
            raise AssertionError('Provided arguments to {} do not match a provided completion'.format(args[0]))

        completion = candidates[0]
        current = cls.top
        while current:
            if current.ordered and completion is current.completions[0]:
                current.completions.pop(0)
                break
            current = current.previous
        return completion(*args, **kwargs)

    def __init__(self, *args, **kwargs):
        if all([isinstance(arg, self.CommandRoute) for arg in args]):
            self.ordered = kwargs.pop('ordered', False)
            if kwargs.keys():
                raise TypeError('__init__() got an unexpected keyword argument {}'.format(kwargs.keys()[0]))
            self.completions = list(args) if self.ordered else sorted(args, key=cmp_to_key(self.CommandRoute.compare))
        elif any([isinstance(arg, self.CommandRoute) for arg in args]):
            raise TypeError('mocks.Subprocess arguments must be of a consistent type')
        else:
            self.ordered = False
            self.completions = [self.CommandRoute(*args, **kwargs)]

        super(Subprocess, self).__init__(cls=Subprocess)

        # Allow mock to be managed via autoinstall
        from mock import patch
        from webkitcorepy.mocks.popen import Popen
        self.patches.append(patch('subprocess.Popen', new=Popen))

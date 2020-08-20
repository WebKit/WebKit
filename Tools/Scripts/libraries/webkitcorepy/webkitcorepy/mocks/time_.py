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

import datetime
import functools
import time


# Without the meta-class, mock.Time must either be a decorator or a contextmanager.
# With the meta-class, mock.Time can be both. Note that it is the meta-class which
# actually owns mocking logic
class _MetaTime(type):

    def __init__(self, *args, **kwargs):
        super(_MetaTime, self).__init__(*args, **kwargs)
        self.patches = []
        self.stack = []

    def __enter__(self):
        # Allow mock to be managed via autoinstall
        from mock import patch
        self.stack.append(time.time())

        def sleep_func(t):
            self.stack[-1] += t

        # datetime.datetime.now is considered a Python builtin, so it can't be mocked.
        # We need to "mock" the whole class to replace a single function.
        class FakeDateTime(datetime.datetime):
            @classmethod
            def now(cls, tz=None):
                return cls.fromtimestamp(self.stack[-1], tz=tz)

        self.patches.append([
            patch('time.time', new=lambda: self.stack[-1]),
            patch('time.sleep', new=sleep_func),
            patch('datetime.datetime', new=FakeDateTime),
            patch('webkitcorepy.timeout.ORIGINAL_SLEEP', new=sleep_func),
        ])

        for patch in self.patches[-1]:
            patch.__enter__()

    def __exit__(self, exc_type, exc_value, traceback):
        for patch in self.patches[-1]:
            patch.__exit__(exc_type, exc_value, traceback)
        self.stack = self.stack[:-1]
        self.patches = self.patches[:-1]


# Python 2/3 compatible metaclass assigner from six
def add_metaclass(metaclass):
    def wrapper(cls):
        orig_vars = cls.__dict__.copy()
        slots = orig_vars.get('__slots__')
        if slots is not None:
            if isinstance(slots, str):
                slots = [slots]
            for slots_var in slots:
                orig_vars.pop(slots_var)
        orig_vars.pop('__dict__', None)
        orig_vars.pop('__weakref__', None)
        if hasattr(cls, '__qualname__'):
            orig_vars['__qualname__'] = cls.__qualname__
        return metaclass(cls.__name__, cls.__bases__, orig_vars)
    return wrapper


class Time(object):
    """
    Mock out time.sleep, time.time and datetime.datetime for testing.
    Like mock.patch, this class can be used as both a decorator and a context manager.

    Example usage:
        @mocks.Time
        def some_function():
            ....

        ...
        with mocks.Time:
            ...
        ...
    """
    __metaclass__ = _MetaTime

    def __init__(self, obj):
        self._callable = obj

    def __get__(self, instance, owner):
        return functools.partial(self.__call__, instance)

    def __call__(self, *args, **kwargs):
        with Time:
            self._callable(*args, **kwargs)


Time = add_metaclass(_MetaTime)(Time)

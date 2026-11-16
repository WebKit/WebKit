# Copyright (C) 2020 Apple Inc. All rights reserved.
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

import functools
import time
from collections import defaultdict
from typing import Any, Callable, Generic, Protocol, TypeVar, cast, overload

R = TypeVar('R')
R_co = TypeVar('R_co', covariant=True)
C = TypeVar('C', bound=Callable[..., Any])


class Memoized(Protocol[R_co]):
    """A function wrapped by Memoize.

    Memoized functions accept `timeout` and `cached` keyword arguments (unless the wrapped
    function declares them itself), which prevents type-checking their other arguments.
    """

    def __call__(self, *args: Any, **kwargs: Any) -> R_co:
        ...

    def clear(self) -> None:
        ...


class Memoize(object):
    def __init__(self, timeout: float | None = None, cached: bool = True) -> None:
        self._cache: defaultdict[Callable[..., Any], dict[tuple[Any, ...], Any]] = defaultdict(dict)
        self._last_called: defaultdict[Callable[..., Any], dict[tuple[Any, ...], float]] = defaultdict(dict)
        self.timeout = timeout
        self.cached = cached

    def __call__(self, function: Callable[..., R]) -> Memoized[R]:
        @functools.wraps(function)
        def decorator(*args: Any, **kwargs: Any) -> R:
            fargs = function.__code__.co_varnames[:function.__code__.co_argcount]

            timeout = self.timeout
            if 'timeout' not in fargs:
                timeout = kwargs.pop('timeout', timeout)

            cached = self.cached
            if 'cached' not in fargs:
                cached = kwargs.pop('cached', cached)

            keyargs = args + tuple(sorted([(key, value) for key, value in kwargs.items()]))
            last_called = self._last_called[function].get(keyargs, 0)
            is_cached = keyargs in self._cache[function]
            if not cached:
                is_cached = False
            if timeout and timeout < time.time() - last_called:
                is_cached = False
            if is_cached:
                result: R = self._cache[function][keyargs]
                return result

            value = function(*args, **kwargs)
            self._last_called[function][keyargs] = time.time()
            self._cache[function][keyargs] = value
            return value

        setattr(decorator, 'clear', self.clear)
        return cast('Memoized[R]', decorator)

    def clear(self) -> None:
        self._cache = defaultdict(dict)
        self._last_called = defaultdict(dict)


class hybridmethod(Generic[C]):
    """A method that receives its instance when called on an instance, or its class when called on the class."""

    @overload
    def __init__(self: hybridmethod[Memoized[R]], function: Memoized[R]) -> None:
        ...

    @overload
    def __init__(self: hybridmethod[Callable[..., R]], function: Callable[..., R]) -> None:
        ...

    def __init__(self, function: Callable[..., Any]) -> None:
        self.function = function

    def __get__(self, obj: object, cls: type | None = None) -> C:
        context = obj if obj is not None else cls

        def wrapper(*args: Any, **kwargs: Any) -> Any:
            return self.function(context, *args, **kwargs)

        wrapper.__name__ = self.function.__name__
        wrapper.__doc__ = self.function.__doc__
        setattr(wrapper, '__func__', self.function)
        setattr(wrapper, 'im_func', self.function)
        setattr(wrapper, '__self__', context)
        setattr(wrapper, 'im_self', context)
        for attribute in ['clear']:
            if getattr(self.function, attribute, None):
                setattr(wrapper, attribute, getattr(self.function, attribute))

        return cast(C, wrapper)

# Copyright (C) 2021 Apple Inc. All rights reserved.
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

import builtins
from typing import Any, Callable, Generic, TypeVar, cast

T = TypeVar('T')


class CallByNeed(Generic[T]):
    def __init__(self, callback: Callable[[], T], type: builtins.type[Any] | None = None) -> None:
        self._callback: Callable[[], T] | None = callback
        self._value: T | None = None
        self.type = type

    def __getattribute__(self, name: str) -> Any:
        if name in dir(type(self)) or name in {'_callback', '_value'}:
            return object.__getattribute__(self, name)
        typ = object.__getattribute__(self, 'type')
        if typ is None or name in dir(typ):
            return object.__getattribute__(self, 'value').__getattribute__(name)
        raise AttributeError("'{}' object has no attribute '{}'".format(typ.__name__, name))

    @property
    def value(self) -> T:
        if self._callback:
            self._value = self._callback()
            self._callback = None
        return cast(T, self._value)

    def __call__(self, *args: Any, **kwargs: Any) -> Any:
        if callable(self.value):
            return self.value(*args, **kwargs)
        return self.value

    def __repr__(self) -> str:
        return self.value.__repr__()

    def __str__(self) -> str:
        return str(self.value)

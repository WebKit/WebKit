# Copyright (C) 2021 Apple Inc. All rights reserved.
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

from __future__ import annotations

import time
from typing import Any

from webkitcorepy import log


class MeasureTime(object):
    def __init__(self, log: bool = False, name: str | None = None) -> None:
        self.started: float | None = None
        self.ended: float | None = None
        self.log = log
        self.name = name

    @property
    def elapsed(self) -> float | None:
        if not self.started:
            return None
        if not self.ended:
            return time.time() - self.started
        return self.ended - self.started

    def __enter__(self) -> MeasureTime:
        self.ended = None
        self.started = time.time()
        return self

    def __exit__(self, *args: Any, **kwargs: Any) -> None:
        self.ended = time.time()
        if self.log and self.name:
            log.critical('{}: {} seconds elapsed'.format(self.name, self.elapsed or 'No'))
        elif self.log:
            log.critical('{} seconds elapsed'.format(self.elapsed or 'No'))

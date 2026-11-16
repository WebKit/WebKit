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

from __future__ import annotations

import contextlib
import io
import logging
import sys
from typing import IO, TYPE_CHECKING, Any, Iterable, NoReturn

from webkitcorepy import log, mocks
from webkitcorepy.string_utils import StringIO


class LoggerCapture(object):
    def __init__(self, logger: logging.Logger | None = None, level: int = logging.WARNING) -> None:
        self.logger = logger or logging.getLogger()
        self.level = level

        self._original_log_level_stack: list[int] = []
        self._original_handlers_stack: list[list[logging.Handler]] = []
        self._handler_stack: list[logging.Handler] = []
        self.log = StringIO()

    def __enter__(self) -> LoggerCapture:
        self._original_log_level_stack.append(self.logger.level)

        self._original_handlers_stack.append([handler for handler in self.logger.handlers])
        for handler in self._original_handlers_stack[-1]:
            self.logger.removeHandler(handler)

        context_handler = logging.StreamHandler(self.log)
        self._handler_stack.append(context_handler)
        context_handler.setLevel(self.level)

        self.logger.addHandler(context_handler)
        self.logger.setLevel(self.level)

        return self

    def __exit__(self, *args: Any, **kwargs: Any) -> None:
        if self._handler_stack:
            to_remove = self._handler_stack.pop()
            to_remove.flush()
            self.logger.removeHandler(to_remove)

        if self._original_log_level_stack:
            self.logger.setLevel(self._original_log_level_stack.pop())

        if self._original_handlers_stack is not None:
            for handler in self._original_handlers_stack.pop():
                self.logger.addHandler(handler)


class OutputCapture(mocks.ContextStack):
    top = None

    class ReplaceSysStream(object):
        def __init__(self, name: str, stream: IO[str] | io.IOBase) -> None:
            self.name = name
            if self.name not in {'stdout', 'stderr'}:
                raise ValueError("'{}' is not a valid system stream name".format(self.name))
            self.stream = stream
            self.originals: list[IO[str] | io.IOBase] = []

        def __enter__(self) -> OutputCapture.ReplaceSysStream:
            self.originals.append(getattr(sys, self.name))
            setattr(sys, self.name, self.stream)
            return self

        def __exit__(self, *args: Any, **kwargs: Any) -> None:
            setattr(sys, self.name, self.originals.pop())

    if TYPE_CHECKING:
        # Each captured logger is available as an attribute named after that logger.
        def __getattr__(self, name: str) -> LoggerCapture:
            ...

    def __init__(self, loggers: Iterable[logging.Logger] | None = None, level: int = logging.WARNING) -> None:
        super(OutputCapture, self).__init__(cls=OutputCapture)

        self.stdout = StringIO()
        self.stderr = StringIO()

        if not loggers:
            loggers = [logging.getLogger(), log]

        self.patches.append(self.ReplaceSysStream('stdout', self.stdout))
        self.patches.append(self.ReplaceSysStream('stderr', self.stderr))

        for logger in loggers:
            if logger.name in ['stdout', 'stderr', 'patches', 'top']:
                raise ValueError('{} collides with an OutputCapture member'.format(logger.name))
            lc = LoggerCapture(logger, level)
            setattr(self, logger.name, lc)
            self.patches.append(lc)


class OutputDuplicate(mocks.ContextStack):
    top = None

    class Stream(io.IOBase):
        def __init__(self, *targets: IO[str]) -> None:
            if not targets:
                raise ValueError('No target streams provided')
            self.targets = targets

        def flush(self) -> None:
            for target in self.targets:
                target.flush()

        # io.IOBase is typed as a binary stream, but this stream duplicates text.
        def writelines(self, lines: Iterable[str]) -> None:  # type: ignore[override]
            for target in self.targets:
                target.writelines(lines)

        def write(self, b: str) -> int:
            for target in self.targets:
                target.write(b)
            return len(b)

        @property
        def closed(self) -> bool:
            return all([target.closed for target in self.targets])

        def close(self) -> None:
            pass

        def fileno(self) -> int | None:  # type: ignore[override]
            # Use the first valid file number
            for target in self.targets:
                if target.fileno():
                    return target.fileno()
            return None

        def isatty(self) -> bool:
            return any([target.isatty() for target in self.targets])

        def readable(self) -> bool:
            return False

        def readline(self, size: int | None = -1) -> NoReturn:
            raise NotImplementedError()

        def readlines(self, hint: int = -1) -> NoReturn:
            raise NotImplementedError()

        def seek(self, offset: int, whence: int = io.SEEK_SET) -> NoReturn:
            raise NotImplementedError()

        def seekable(self) -> bool:
            return False

        def tell(self) -> NoReturn:
            raise NotImplementedError()

        def truncate(self, size: int | None = None) -> NoReturn:
            raise NotImplementedError()

        def writable(self) -> bool:
            return True

    def __init__(self, loggers: list[logging.Logger] | None = None) -> None:
        super(OutputDuplicate, self).__init__(cls=OutputDuplicate)

        self.output = StringIO()
        self.loggers = loggers or [logging.getLogger()]

        self._handler_stack: list[logging.Handler] = []
        self._streams_stack: list[tuple[OutputCapture.ReplaceSysStream, OutputCapture.ReplaceSysStream]] = []

    def __enter__(self) -> OutputDuplicate:
        self._streams_stack.append((
            OutputCapture.ReplaceSysStream('stdout', self.Stream(sys.stdout, self.output)).__enter__(),
            OutputCapture.ReplaceSysStream('stderr', self.Stream(sys.stderr, self.output)).__enter__(),
        ))

        context_handler = logging.StreamHandler(self.output)
        self._handler_stack.append(context_handler)
        context_handler.setLevel(min([logger.level for logger in self.loggers]))
        for logger in self.loggers:
            logger.addHandler(context_handler)

        return super(OutputDuplicate, self).__enter__()

    def __exit__(self, *args: Any, **kwargs: Any) -> None:
        super(OutputDuplicate, self).__exit__(*args, **kwargs)

        context_handler = self._handler_stack.pop()
        context_handler.flush()
        for logger in self.loggers:
            logger.removeHandler(context_handler)

        stdout_stream, stderr_stream = self._streams_stack.pop()
        stdout_stream.__exit__(*args, **kwargs)
        stderr_stream.__exit__(*args, **kwargs)

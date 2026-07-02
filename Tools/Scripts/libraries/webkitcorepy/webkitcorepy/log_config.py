# Copyright (C) 2026 Apple Inc. All rights reserved.
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

import logging
import sys

from webkitcorepy import Terminal


class ColorFormatter(logging.Formatter):
    def __init__(self, fmt: str, use_color: bool = True) -> None:
        super().__init__(fmt)
        self._use_color = use_color

    def format(self, record: logging.LogRecord) -> str:
        if self._use_color:
            if record.levelno >= logging.ERROR:
                color = Terminal.Text.red
            elif record.levelno >= logging.WARNING:
                color = Terminal.Text.yellow
            else:
                color = None
            if color:
                record = logging.makeLogRecord(record.__dict__)
                record.levelname = f'{color}{record.levelname}{Terminal.Text.reset}'
                record.msg = f'{color}{record.msg}{Terminal.Text.reset}'
        return super().format(record)


class LogConfig:
    DEFAULT_FORMAT = '%(asctime)s - %(levelname)s - %(message)s'
    VERBOSE_FORMAT = '%(asctime)s - %(levelname)s - %(module)s.%(funcName)s:%(lineno)d - %(message)s'

    def __init__(self, name: str) -> None:
        use_color = Terminal.supports_color(sys.stderr)

        self._default_fmt = ColorFormatter(self.DEFAULT_FORMAT, use_color=use_color)
        self._verbose_fmt = ColorFormatter(self.VERBOSE_FORMAT, use_color=use_color)

        self._logger = logging.getLogger(name)
        self._logger.propagate = False

        stdout_handler = logging.StreamHandler(sys.stdout)
        stdout_handler.addFilter(lambda record: record.levelno < logging.WARNING)
        stdout_handler.setFormatter(self._default_fmt)
        self._logger.addHandler(stdout_handler)

        stderr_handler = logging.StreamHandler(sys.stderr)
        stderr_handler.addFilter(lambda record: record.levelno >= logging.WARNING)
        stderr_handler.setFormatter(self._default_fmt)
        self._logger.addHandler(stderr_handler)

    @property
    def logger(self) -> logging.Logger:
        return self._logger

    def set_verbose(self, enabled: bool) -> None:
        fmt = self._verbose_fmt if enabled else self._default_fmt
        for handler in self._logger.handlers:
            handler.setFormatter(fmt)

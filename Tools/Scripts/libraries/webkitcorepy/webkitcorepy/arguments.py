# Copyright (C) 2020-2026 Apple Inc. All rights reserved.
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

import argparse
import logging
from typing import TYPE_CHECKING, Any, Callable, Sequence

from webkitcorepy import log

if TYPE_CHECKING:
    from webkitcorepy.log_config import LogConfig


class NoAction(argparse.Action):
    def __init__(self, option_strings: Sequence[str], dest: str, **kwargs: Any) -> None:
        super(NoAction, self).__init__(option_strings, dest, nargs=0, **kwargs)

    def __call__(self, parser: argparse.ArgumentParser, namespace: argparse.Namespace, values: Any, option_string: str | None = None) -> None:
        # NoAction is only used for optional arguments, which always have an option string.
        assert option_string is not None
        setattr(namespace, self.dest, not any((
            option_string.startswith('--no-'),
            option_string.startswith('--un'),
            option_string.startswith('--skip-'),
        )))


def CountAction(value: int = 1) -> type[argparse.Action]:
    class Action(argparse.Action):
        def __init__(self, option_strings: Sequence[str], dest: str, **kwargs: Any) -> None:
            super(Action, self).__init__(option_strings, dest, nargs=0, **kwargs)

        def __call__(self, parser: argparse.ArgumentParser, namespace: argparse.Namespace, values: Any, option_string: str | None = None) -> None:
            setattr(namespace, self.dest, getattr(namespace, self.dest) + value)

    return Action


def CallbackAction(action: type[argparse.Action], callback: Callable[[argparse.Namespace], None] = lambda namespace: None) -> type[argparse.Action]:
    # mypy can't check a base class that's only known at runtime.
    class Action(action):  # type: ignore[misc,valid-type]
        def __call__(self, parser: argparse.ArgumentParser, namespace: argparse.Namespace, values: Any, option_strings: str | None = None) -> None:
            super(Action, self).__call__(parser, namespace, values, option_strings)
            callback(namespace)

    return Action


def LoggingGroup(
    parser: argparse.ArgumentParser, loggers: Sequence[logging.Logger] | None = None, log_config: LogConfig | None = None,
    default: int = logging.WARNING, help: str = '{} amount of logging',
) -> argparse._ArgumentGroup:
    if not isinstance(parser, argparse.ArgumentParser):
        raise ValueError('Provided parser is not a {}'.format(type(argparse.ArgumentParser)))

    if not loggers:
        loggers = [logging.getLogger(), log]
    for logger in loggers:
        logger.setLevel(default)

    def verbose_callback(namespace: argparse.Namespace) -> None:
        verbosity = getattr(namespace, 'verbose')
        log_level = default - verbosity * 10

        setattr(namespace, 'log_level', log_level)

        for logger in loggers:
            logger.setLevel(log_level)

        if log_config:
            log_config.set_verbose(log_level <= logging.DEBUG)

    group = parser.add_argument_group('Logging')
    group.add_argument(
        '--verbose', '-v',
        dest='verbose', default=0,
        help=help.format('Increase'),
        action=CallbackAction(CountAction(value=1), callback=verbose_callback),
    )
    group.add_argument(
        '--quiet', '-q',
        dest='verbose', default=0,
        help=help.format('Decrease'),
        action=CallbackAction(CountAction(value=-1), callback=verbose_callback),
    )
    return group

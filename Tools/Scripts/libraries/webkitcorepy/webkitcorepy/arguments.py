# Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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

import argparse
import logging

from webkitcorepy import log


class NoAction(argparse.Action):
    def __init__(self, option_strings, dest, **kwargs):
        super(NoAction, self).__init__(option_strings, dest, nargs=0, **kwargs)

    def __call__(self, parser, namespace, values, option_string=None):
        setattr(namespace, self.dest, not any((
            option_string.startswith('--no-'),
            option_string.startswith('--un'),
            option_string.startswith('--skip-'),
        )))


def CountAction(value=1):
    class Action(argparse.Action):
        def __init__(self, option_strings, dest, **kwargs):
            super(Action, self).__init__(option_strings, dest, nargs=0, **kwargs)

        def __call__(self, parser, namespace, values, option_string):
            setattr(namespace, self.dest, getattr(namespace, self.dest) + value)

    return Action


def CallbackAction(action, callback=lambda namespace: None):
    class Action(action):
        def __call__(self, parser, namespace, values, option_strings):
            super(Action, self).__call__(parser, namespace, values, option_strings)
            callback(namespace)

    return Action


def LoggingGroup(parser, loggers=None, default=logging.WARNING, help='{} amount of logging'):
    if not isinstance(parser, argparse.ArgumentParser):
        raise ValueError('Provided parser is not a {}'.format(type(argparse.ArgumentParser)))

    if not loggers:
        loggers = [logging.getLogger(), log]
    for logger in loggers:
        logger.setLevel(default)

    def verbose_callback(namespace):
        verbosity = getattr(namespace, 'verbose')
        log_level = default - verbosity * 10

        setattr(namespace, 'log_level', log_level)

        for logger in loggers:
            logger.setLevel(log_level)

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

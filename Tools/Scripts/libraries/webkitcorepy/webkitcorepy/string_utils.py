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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import io


basestring = str
BytesIO = io.BytesIO
StringIO = io.StringIO
UnicodeIO = io.StringIO

unicode = str


def encode(string, encoding='utf-8', errors='strict', target_type=bytes):
    if type(string) == unicode and target_type == bytes:
        return string.encode(encoding, errors=errors)
    return string


def decode(data, encoding='utf-8', errors='strict', target_type=unicode):
    if type(data) == bytes and target_type == unicode:
        return data.decode(encoding, errors=errors)
    return data


def ordinal(number):
    number = int(number)
    if 10 < number % 100 < 20:
        return '{}th'.format(number)
    return '{}{}'.format(
        number, {
            1: 'st',
            2: 'nd',
            3: 'rd',
        }.get(number % 10, 'th')
    )


def pluralize(number, string, plural=None):
    if number == 1:
        return '1 {}'.format(string)
    if plural:
        return '{} {}'.format(number, plural)
    return '{} {}s'.format(number, string)


def join(list, conjunction='and'):
    if not list:
        return 'Nothing'
    if len(list) == 1:
        return list[0]
    conjunctionWithSerialCommaIfNeeded = f'{"," if len(list) > 2 else ""} {conjunction} '
    return '{}{}{}'.format(', '.join(list[:-1]), conjunctionWithSerialCommaIfNeeded, list[-1])


def split(string, conjunctions=None):
    conjunctions = ['and', 'or']
    if not string:
        return []

    result = [string]
    for conjunction in conjunctions:
        conjunction = ' {} '.format(conjunction)
        result = [clause.strip() for phrase in result for clause in phrase.split(conjunction) if clause.strip()]

    return [word.strip() for clause in result for word in clause.split(',') if word.strip()]


def out_of(number, base):
    number = str(number)
    base = str(base)
    return '[{}{}/{}]'.format(' ' * (len(base) - len(number)), number, base)


def elapsed(seconds: float, precision: int = None, shorthand: bool = False):
    rounded_seconds = round(seconds, ndigits=precision)
    if seconds < 60:
        if shorthand:
            return f'{rounded_seconds}s' if not (rounded_seconds == 0 and seconds > 0) else '<1s'
        elif precision is None:
            if seconds <= 0:
                return 'no time'
            elif seconds < 1:
                return 'less than a second'
        return pluralize(rounded_seconds, 'second')

    seconds = int(seconds)
    minutes = seconds // 60
    hours = minutes // 60
    days = hours // 24

    precise_seconds = (seconds % 60) + round(rounded_seconds - seconds, ndigits=precision)

    force = False
    result = ''
    for value, description, letter in [(days, 'day', 'd'), (hours % 24, 'hour', 'h'), (minutes % 60, 'minute', 'm')]:
        if force or value:
            force = True
            result += f'{str(value)}{letter} ' if shorthand else f' {pluralize(value, description)}'
    if force:
        return result + f'{precise_seconds}s' if shorthand else f"{result[1:]} and {pluralize(precise_seconds, 'second')}"
    return pluralize(precise_seconds, 'second')

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
import sys

BytesIO = io.BytesIO
if sys.version_info > (3, 0):
    StringIO = io.StringIO
else:
    from StringIO import StringIO
UnicodeIO = io.StringIO

unicode = str if sys.version_info > (3, 0) else unicode


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
    return '{} {} {}'.format(', '.join(list[:-1]), conjunction, list[-1])


def out_of(number, base):
    number = str(number)
    base = str(base)
    return '[{}{}/{}]'.format(' ' * (len(base) - len(number)), number, base)


def elapsed(seconds):
    if seconds <= 0:
        return 'no time'
    elif seconds < 1:
        return 'less than a second'

    seconds = int(round(seconds))
    minutes = seconds // 60
    hours = minutes // 60
    days = hours // 24

    force = False
    result = ''
    for value, description in [(days, 'day'), (hours % 24, 'hour'), (minutes % 60, 'minute')]:
        if force or value:
            force = True
            result += ' ' + pluralize(value, description)
    if force:
        return result[1:] + ' and ' + pluralize(seconds % 60, 'second')
    return pluralize(seconds % 60, 'second')

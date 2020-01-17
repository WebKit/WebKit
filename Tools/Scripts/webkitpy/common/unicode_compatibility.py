# Copyright (C) 2019 Apple Inc. All rights reserved.
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


def encode_if_necessary(value, encoding='utf-8', errors='strict'):
    # In Python 3, string types must be encoded
    if type(value) == unicode:
        return value.encode(encoding, errors=errors)
    return value


def encode_for(value, target_type, **kwargs):
    if target_type == bytes:
        return encode_if_necessary(value, **kwargs)

    if sys.version_info < (3, 0) and target_type == str:
        return encode_if_necessary(value, **kwargs)
    return value


def decode_if_necessary(value, encoding='utf-8', errors='strict'):
    # In Python 2, string types might need to be decoded
    if type(value) == bytes:
        return value.decode(encoding, errors=errors)
    return value


def decode_for(value, target_type):
    if value is None:
        return None
    if type(value) == target_type:
        return value
    if target_type == unicode:
        return decode_if_necessary(bytes(value))
    return value

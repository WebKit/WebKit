# Copyright (C) 2018 Apple Inc. All rights reserved.
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

import os

_cached_authorized_api_keys = frozenset([])
_AUTHORIZATION_SCHEME = "apikey"


def _path_to_authorized_api_keys_file():
    return os.path.abspath(os.path.join(os.path.dirname(__file__), "authorized_api_keys.txt"))


def _parse_authorized_api_keys(file):
    api_keys = set()
    for line in file:
        line = line.strip()
        if not line or line.startswith("#"):  # Skip empty lines and comments
            continue
        api_keys.add(line)
    return frozenset(api_keys)


def authorized_api_keys():
    global _cached_authorized_api_keys
    if _cached_authorized_api_keys:
        return _cached_authorized_api_keys

    with open(_path_to_authorized_api_keys_file(), "r") as file:
        _cached_authorized_api_keys = _parse_authorized_api_keys(file)
    return _cached_authorized_api_keys


def _parse_authorization_header(credentials):
    # See <https://tools.ietf.org/html/rfc7235#section-4.2>.
    parts = credentials.split(" ", 1)
    if len(parts) < 2:
        return ""
    scheme = parts[0]
    token68_encoded_value = parts[1]
    if scheme.lower() == _AUTHORIZATION_SCHEME:
        return token68_encoded_value
    return ""


def is_authorized(request):
    api_key = ''
    credentials = request.headers.get("Authorization")
    if credentials:
        api_key = _parse_authorization_header(credentials)
    if not api_key:
        api_key = request.get(_AUTHORIZATION_SCHEME)
    return api_key in authorized_api_keys()

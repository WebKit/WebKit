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

import cgi
import os
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
post_data = {}


def get_cookies():
    cookies = {}
    if 'HTTP_COOKIE' in os.environ:
        header_cookies = os.environ['HTTP_COOKIE']
        header_cookies = header_cookies.split('; ')

        for cookie in header_cookies:
            cookie = cookie.split('=')
            cookies[cookie[0]] = cookie[1]

    return cookies


def get_post_data():
    request_method = os.environ.get('REQUEST_METHOD', '')
    if request_method == 'POST':
        form = cgi.FieldStorage()
        for key in form.keys():
            if key not in query.keys():
                post_data.update({key: form.getvalue(key)})

    return post_data


def get_request():
    request = {}
    for key in query.keys():
        request.update({key: query[key][0]})

    # request.update(get_post_data())
    request.update(get_cookies())
    return request


def get_count(file):
    if not os.path.isfile(file):
        with open(file, 'w') as open_file:
            open_file.write('0')
            return '0'

    with open(file, 'r') as open_file:
        return open_file.read()


def get_state(file, default='Uninitialized'):
    if not os.path.isfile(file):
        return default
    with open(file, 'r') as file:
        return file.read()


def set_state(file, state):
    with open(file, 'w') as open_file:
        open_file.write(state)
    return state


def step_state(file):
    state = get_count(file)
    with open(file, 'w') as open_file:
        open_file.write(f'{int(state) + 1}')
    return state

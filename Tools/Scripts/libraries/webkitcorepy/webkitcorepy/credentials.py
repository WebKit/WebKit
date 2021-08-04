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

import os
import getpass
import sys

from subprocess import CalledProcessError
from webkitcorepy import OutputCapture, Terminal

_cache = dict()


def credentials(url, required=True, name=None, prompt=None, key_name='password'):
    global _cache

    name = name or url.split('/')[2].replace('.', '_')
    if _cache.get(name):
        return _cache.get(name)

    username = os.environ.get('{}_USERNAME'.format(name.upper()))
    key = os.environ.get('{}_{}'.format(name.upper(), name.upper()))

    if username and key:
        _cache[name] = (username, key)
        return username, key

    with OutputCapture():
        try:
            import keyring
        except (CalledProcessError, ImportError):
            keyring = None

    username_prompted = False
    key_prompted = False
    if not username:
        try:
            if keyring:
                username = keyring.get_password(url, 'username')
        except (RuntimeError, AttributeError):
            pass

        if not username and required:
            if not sys.stderr.isatty() or not sys.stdin.isatty():
                raise OSError('No tty to prompt user for username')
            sys.stderr.write("Authentication required to use {}\n".format(prompt or name))
            sys.stderr.write('Username: ')
            username = Terminal.input()
            username_prompted = True

    if not key:
        try:
            if keyring:
                key = keyring.get_password(url, username)
        except (RuntimeError, AttributeError):
            pass

        if not key and required:
            if not sys.stderr.isatty() or not sys.stdin.isatty():
                raise OSError('No tty to prompt user for username')
            key = getpass.getpass('{}: '.format(key_name.capitalize()))
            key_prompted = True

    if username and key:
        _cache[name] = (username, key)

    if keyring and (username_prompted or key_prompted):
        sys.stderr.write('Store username and {} in system keyring for {}? (Y/N): '.format(key_name, url))
        response = Terminal.input()
        if response.lower() in ['y', 'yes', 'ok']:
            sys.stderr.write('Storing credentials...\n')
            keyring.set_password(url, 'username', username)
            keyring.set_password(url, username, key)
        else:
            sys.stderr.write('Credentials cached in process.\n')

    return username, key

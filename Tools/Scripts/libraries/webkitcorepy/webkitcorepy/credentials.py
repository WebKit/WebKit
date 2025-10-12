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

import getpass
import sys

from subprocess import CalledProcessError
from webkitcorepy import Environment, OutputCapture, Terminal, string_utils

_cache = dict()


def handle_keyring_error(error):
    sys.stderr.write('Could not access credentials from keychain. Please run `security unlock-keychain` before re-running this command.\n')
    sys.exit(1)


def credentials(url, required=True, name=None, prompt=None, key_name='password', validater=None, validate_existing_credentials=False, retry=3, save_in_keyring=None):
    global _cache

    ignore_entry = False
    name = name or url.split('/')[2].replace('.', '_')
    if _cache.get(name):
        if not validate_existing_credentials:
            return _cache[name]
        elif validater and validater(*_cache.get(name)):
            return _cache[name]

        # If we've failed the validation check, invalidate cache and ignore the current keychain entry
        del _cache[name]
        ignore_entry = True

    username = Environment.instance().get('{}_USERNAME'.format(name.upper()))
    key = Environment.instance().get('{}_{}'.format(name.upper(), key_name.upper()))

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

    for attempt in range(retry):
        if not attempt and ignore_entry:
            continue
        if attempt:
            sys.stderr.write('Ignoring keychain values and re-prompting user\n')
        if not username:
            try:
                if keyring and not attempt:
                    username = keyring.get_password(url, 'username')
            except (RuntimeError, AttributeError):
                pass
            except keyring.errors.KeyringError as e:
                handle_keyring_error(e)

            if not username and required:
                if not sys.stderr.isatty() or not sys.stdin.isatty():
                    raise OSError('No tty to prompt user for username')
                if prompt and callable(prompt):
                    prompt = prompt()
                sys.stderr.write("Authentication required to use {}\n".format(prompt or name))
                sys.stderr.write('Username: ')
                username = Terminal.input().strip()
                username_prompted = True

        if not key and username:
            try:
                if keyring and not attempt:
                    key = keyring.get_password(url, username)
            except (RuntimeError, AttributeError):
                pass
            except keyring.errors.KeyringError as e:
                handle_keyring_error(e)

            if not key and required:
                if not sys.stderr.isatty() or not sys.stdin.isatty():
                    raise OSError('No tty to prompt user for username')
                key = getpass.getpass('{}: '.format(key_name.capitalize())).strip()
                key_prompted = True

        should_validate = validater and (username_prompted or key_prompted or validate_existing_credentials)
        if username and key and (not should_validate or validater(username, key)):
            _cache[name] = (username, key)
            break

        username = None
        key = None

        if not required:  # Failed validation, but credentials aren't required
            break

        sys.stderr.write("Failed to validate credentials for '{}' ({} attempt)\n".format(url, string_utils.ordinal(attempt + 1)))

    if required and (not username or not key):
        sys.stderr.write("Exhausted attempts to prompt user for '{}' credentials\n".format(url))
        sys.exit(1)

    if keyring and (username_prompted or key_prompted):
        if save_in_keyring or (save_in_keyring is None and Terminal.choose(
            'Store username and {} in system keyring for {}?'.format(key_name, url),
            default='Yes',
        ) == 'Yes'):
            sys.stderr.write('Storing credentials...\n')
            try:
                keyring.set_password(url, 'username', username)
                keyring.set_password(url, username, key)
            except keyring.errors.KeyringError as e:
                handle_keyring_error(e)
        else:
            sys.stderr.write('Credentials cached in process.\n')

    return username, key


def delete_credentials(url, name=None):
    global _cache

    name = name or url.split('/')[2].replace('.', '_')
    if name in _cache:
        del _cache[name]

    with OutputCapture():
        try:
            import keyring

            username = None
            try:
                if keyring:
                    username = keyring.get_password(url, 'username')
            except (RuntimeError, AttributeError):
                pass

            for key in ['username', username]:
                if key:
                    keyring.delete_password(url, key)

        except (CalledProcessError, ImportError):
            pass

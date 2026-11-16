# Copyright (C) 2025 Apple Inc. All rights reserved.
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

import json
import os
import socket


def load_password(name, default=None, master_prefix_path=os.path.dirname(os.path.abspath(__file__))):
    if os.getenv(name):
        value = os.getenv(name)
        # If the default is a list, dict, or bool, try to parse the env var as JSON
        if isinstance(default, (list, dict)):
            try:
                return json.loads(value)
            except (json.JSONDecodeError, TypeError):
                return value
        elif isinstance(default, bool):
            # Handle boolean strings
            if value.lower() in ('true', '1', 'yes'):
                return True
            elif value.lower() in ('false', '0', 'no'):
                return False
            return value
        return value
    try:
        passwords = json.load(open(os.path.join(master_prefix_path, 'passwords.json')))
        return passwords.get(name, default)
    except FileNotFoundError as e:
        print(f'Using default value for {name}, {e}')
    except Exception as e:
        print(f'Error in finding {name} in passwords.json: {e}')
    return default


def get_custom_suffix():
    hostname = socket.gethostname().strip()
    if 'dev' in hostname:
        return '-dev'
    if 'uat' in hostname:
        return '-uat'
    return ''

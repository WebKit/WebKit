#!/usr/bin/env python3
import base64
import os
import sys
import tempfile

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file)))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import set_state, get_state
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Cache-Control: no-cache, no-store, must-revalidate\r\n'
    'Pragma: no-cache\r\n'
)

if not tempfile.gettempdir():
    sys.stdout.write(
        '\r\n'
        'FAIL: No temp dir was returned.\n'
    )
    sys.exit('0')


stateFile = os.path.join(tempfile.gettempdir(), 'remember-bad-password-status')

command = query.get('command', [''])[0]
if command:
    sys.stdout.write('\n')
    if command == 'status':
        sys.stdout.write(get_state(stateFile, default='0'))
    elif command == 'reset':
        sys.stdout.write(set_state(stateFile, '0'))
    sys.exit(0)

credentials = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')
username = credentials[0]
password = ':'.join(credentials[1:])

if username and username == query.get('uid', [''])[0]:
    sys.stdout.write(
        '\r\n'
        'User: {}, password: {}.'.format(username, password)
    )

else:
    sys.stdout.write(
        'WWW-Authenticate: Basic realm="WebKit Test Realm"\r\n'
        'status: 401\r\n'
        '\r\n'
        'Authentication canceled'
    )
    if username:
        set_state(stateFile, str(int(get_state(stateFile, default='0')) + 1))

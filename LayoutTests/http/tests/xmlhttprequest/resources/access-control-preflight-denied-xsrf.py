#!/usr/bin/env python3
import os
import sys
import tempfile

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import set_state, get_state
from urllib.parse import parse_qs


def fail(state):
    sys.stdout.write(
        'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
        'Access-Control-Allow-Credentials: true\r\n'
        'Access-Control-Allow-Methods: GET\r\n'
        'Access-Control-Max-Age: 1\r\n'
        '\r\n'
        "FAIL: Issued a {} request during state '{}'".format(
            os.environ.get('REQUEST_METHOD', '?'),
            state,
        ))
    sys.exit(0)

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
stateFile = os.path.join(tempfile.gettempdir(), query.get('test', ['state.txt'])[0])
state = get_state(stateFile)
stateArg = query.get('state', [None])[0]

sys.stdout.write('Content-Type: text/html\r\n')

if os.environ.get('REQUEST_METHOD') == 'GET' and stateArg == 'reset':
    if os.path.isfile(stateFile):
        os.remove(stateFile)
    sys.stdout.write(
        'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
        'Access-Control-Max-Age: 1\r\n'
        '\r\n'
        'Server state reset.\n'
    )

elif state == 'Uninitialized':
    if os.environ.get('REQUEST_METHOD') == 'OPTIONS':
        if stateArg in ['method', 'header']:
            sys.stdout.write(
                'Access-Control-Allow-Methods: GET\r\n'
                'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
                'Access-Control-Max-Age: 1\r\n'
            )
        sys.stdout.write(
            '\r\n'
            'FAIL: This request should not be displayed\n'
        )
        set_state(stateFile, 'Denied')
    else:
        fail(state)

elif state == 'Denied':
    if os.environ.get('REQUEST_METHOD') == 'GET' and stateArg == 'complete':
        os.remove(stateFile)
        sys.stdout.write(
            'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
            'Access-Control-Max-Age: 1\r\n'
            '\r\n'
            'PASS: Request successfully blocked.\n'
        )
    else:
        set_state(stateFile, 'Deny Ignored')
        fail(state)

elif state == 'Deny Ignored':
    os.remove(stateFile)
    fail(state)

else:
    if os.path.isfile(stateFile):
        os.remove(stateFile)
    fail('Unknown')

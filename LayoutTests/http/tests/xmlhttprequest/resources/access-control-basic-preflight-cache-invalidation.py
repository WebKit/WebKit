#!/usr/bin/env python3
import os
import sys
import tempfile

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import set_state, get_state
from urllib.parse import parse_qs


def fail():
    sys.stdout.write(
        'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
        'Access-Control-Allow-Credentials: true\r\n'
        'Access-Control-Allow-Methods: PUT\r\n'
        'Access-Control-Allow-Headers: x-webkit-test\r\n'
        '\r\n'
        'FAIL: {}\n'.format(os.environ.get('REQUEST_METHOD', '?'))
    )
    sys.exit(0)

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
stateFile = os.path.join(tempfile.gettempdir(), query.get('filename', ['state.txt'])[0])
state = get_state(stateFile)

sys.stdout.write('Content-Type: text/html\r\n')
if state == 'Uninitialized':
    if os.environ.get('REQUEST_METHOD') == 'OPTIONS':
        sys.stdout.write(
            'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
            'Access-Control-Allow-Credentials: true\r\n'
            'Access-Control-Allow-Methods: PUT\r\n'
            'Access-Control-Max-Age: 10\r\n'
            '\r\n'
        )
        set_state(stateFile, 'OptionsSent')
    else:
        fail()

elif state == 'OptionsSent':
    if os.environ.get('REQUEST_METHOD') == 'PUT':
        sys.stdout.write(
            'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
            'Access-Control-Allow-Credentials: true\r\n'
            '\r\n'
            'PASS: First PUT request.'
        )
        set_state(stateFile, 'FirstPUTSent')
    else:
        fail()

elif state == 'FirstPUTSent':
    if os.environ.get('REQUEST_METHOD') == 'OPTIONS':
        sys.stdout.write(
            'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
            'Access-Control-Allow-Credentials: true\r\n'
            'Access-Control-Allow-Methods: PUT, XMETHOD\r\n'
            'Access-Control-Allow-Headers: x-webkit-test\r\n'
            '\r\n'
        )
        set_state(stateFile, 'SecondOPTIONSSent')
    elif os.environ.get('REQUEST_METHOD') == 'PUT':
        sys.stdout.write(
            'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
            'Access-Control-Allow-Credentials: true\r\n'
            '\r\n'
            'FAIL: Second PUT request sent without preflight'
        )
    else:
        fail()

elif state == 'SecondOPTIONSSent':
    if os.environ.get('REQUEST_METHOD') in ['PUT', 'XMETHOD']:
        sys.stdout.write(
            'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
            'Access-Control-Allow-Credentials: true\r\n'
            '\r\n'
            'PASS: Second OPTIONS request was sent.'
        )
    else:
        fail()

else:
    fail()

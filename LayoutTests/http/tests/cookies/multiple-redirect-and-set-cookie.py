#!/usr/bin/env python3

import hashlib
import os
import sys
from datetime import datetime, timedelta
from urllib.parse import parse_qs

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.abspath(os.path.dirname(file)))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies

current_time = datetime.utcnow()
expires = current_time + timedelta(seconds=1)

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
cookie_name = query.get('cookie_name', [hashlib.md5(bytes('{}{}'.format(__file__, current_time), 'utf-8')).hexdigest()])[0]
step = query.get('step', [None])[0]

cookies = get_cookies()


def redirect_url(step):
    return 'http://127.0.0.1:8000/cookies/{}?step={}&cookie_name={}'.format(os.path.basename(__file__), step, cookie_name)


def redirect_to_step(step):
    sys.stdout.write(
        'status: 302\r\n'
        'Location: {}\r\n\r\n'.format(redirect_url(step))
    )


def step0():
    sys.stdout.write(
        '<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">\n'
        '<html>\n'
        '<script>\n'
        'if (window.testRunner) {{\n'
        '    testRunner.dumpAsText();\n'
        '    testRunner.waitUntilDone();\n'
        '}}\n'
        '\n'
        'function gotoStep1() {{\n'
        '    window.location = "{}";\n'
        '}}\n'
        '</script>\n'
        '\n'
        '<body onload="gotoStep1()">\n'
        '</body>\n'
        '</html>\n'.format(redirect_url(1))
    )


def step2(result):
    sys.stdout.write(
        '<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">\n'
        '<html>\n'
        '<script>\n'
        'function finish() {{\n'
        '    if (window.testRunner)\n'
        '        testRunner.notifyDone();\n'
        '}}\n'
        '</script>\n'
        '\n'
        '<body onload="finish()">\n'
        'Cookie: {}\n'
        '</body>\n'
        '</html>\n'.format(result)
    )


sys.stdout.write('Content-Type: text/html\r\n')
if step is None:
    # Step 0: Set cookie for following request.
    sys.stdout.write(
        'Set-Cookie: {}=not+sure%2C+but+something; expires={} GMT; Max-Age=30\r\n\r\n'.format(cookie_name, expires.strftime('%a, %d-%b-%Y %H:%M:%S')))
    step0()
elif int(step) == 1:
    # Step 1: Request caused by JS. It is sent with Cookie header with value of step 0.
    sys.stdout.write('Set-Cookie: {}=42; expires={} GMT; Max-Age=30\r\n'.format(cookie_name, expires.strftime('%a, %d-%b-%Y %H:%M:%S')))
    redirect_to_step(2)
elif int(step) == 2:
    # Step 2: Redirected request should have only Cookie header with update value.
    sys.stdout.write('\r\n')
    step2(cookies.get(cookie_name, ''))
else:
    sys.stdout.write('\r\n')
    sys.exit('Error: unknown step: {}'.format(step))

sys.exit(0)

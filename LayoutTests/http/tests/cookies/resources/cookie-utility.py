#!/usr/bin/env python3

import os
import sys
from datetime import datetime, timedelta
from urllib.parse import parse_qs

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies

cookies = get_cookies()

def delete_cookie(name):
    expires = datetime.utcnow() - timedelta(seconds=86400)
    sys.stdout.write('Set-Cookie: {}=deleted; expires={} GMT; Max-Age=0; path=/\r\n'.format(name, expires.strftime('%a, %d-%b-%Y %H:%M:%S')))

query_function = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('queryfunction', [''])[0]

sys.stdout.write('Content-Type: text/html\r\n')

if query_function == 'deleteCookiesAndPostMessage':
    for cookie in cookies.keys():
        delete_cookie(cookie)
    sys.stdout.write('\r\n<script>window.opener.postMessage(\'done\', \'*\');</script>\n')
    
elif query_function == 'deleteCookies':
    for cookie in cookies.keys():
        delete_cookie(cookie)
    sys.stdout.write('\r\nDeleted all cookies')
    sys.exit(0)
    
elif query_function == 'setFooCookie':
    expires = datetime.utcnow() + timedelta(seconds=86400)
    sys.stdout.write(
        'Set-Cookie: foo=awesomevalue; expires={} GMT; Max-Age=86400; path=/\r\n\r\n'
        'Set the foo cookie'.format(expires.strftime('%a, %d-%b-%Y %H:%M:%S'))
    )
    sys.exit(0)
    
elif query_function == 'setFooAndBarCookie':
    expires = datetime.utcnow() + timedelta(seconds=86400)
    sys.stdout.write(
        'Set-Cookie: foo=awesomevalue; expires={expires} GMT; Max-Age=86400; path=/\r\n'
        'Set-Cookie: bar=anotherawesomevalue; expires={expires} GMT; Max-Age=86400; path=/\r\n\r\n'
        'Set the foo and bar cookies'.format(expires=expires.strftime('%a, %d-%b-%Y %H:%M:%S'))
    )
    sys.exit(0)
    
else:
    sys.stdout.write('\r\nCookies are:\n')
    for cookie in cookies:
        cookie = cookie.split('=')
        sys.stdout.write('{} = {}\n'.format(cookie[0], cookie[1]))
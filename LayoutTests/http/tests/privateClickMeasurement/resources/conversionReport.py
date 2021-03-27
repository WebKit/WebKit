#!/usr/bin/env python3

import os
import sys
from conversionFilePath import conversion_file_path

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

request_body = ''.join(sys.stdin.readlines())
conversion_file = open('{}.tmp'.format(conversion_file_path), 'w')
cookies_found = False

cookie = os.environ.get('HTTP_COOKIE', None)
content_type = os.environ.get('CONTENT_TYPE', None)
host = os.environ.get('HTTP_HOST', None)
uri = os.environ.get('REQUEST_URI', None)

if host is not None:
    conversion_file.write('HTTP_HOST: {}\n'.format(host))

if cookie is not None:
    conversion_file.write('Cookies in attribution request: {}\n'.format(cookie))
    cookies_found = True

if content_type is not None:
    conversion_file.write('Content type: {}\n'.format(content_type))

if uri is not None:
    position_of_nonce = uri.find('?nonce=')
    position_of_nonce_alternate = uri.find('&nonce=')

    if position_of_nonce == -1:
        output_url = uri
    else:
        output_url = uri[0:position_of_nonce]

    if position_of_nonce_alternate != -1:
        output_url = uri[0:position_of_nonce_alternate]

    conversion_file.write('REQUEST_URI: {}\n'.format(output_url))

if not cookies_found:
    conversion_file.write('No cookies in attribution request.\n')

conversion_file.write('Request body:\n{}\n'.format(request_body))
conversion_file.close()

if os.path.isfile('{}.tmp'.format(conversion_file_path)):
    os.rename('{}.tmp'.format(conversion_file_path), conversion_file_path)

sys.stdout.write(
    'status: 200\r\n'
    'Set-Cookie: cookieSetInConversionReport=1; path=/\r\n'
    'Content-Type: text/html\r\n\r\n'
)

#!/usr/bin/env python3

import os
import sys
from tokenSigningFilePath import token_signing_filepath

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

token_signing_file = open('{}.tmp'.format(token_signing_filepath), 'a')
cookies_found = False
is_last_request = False
is_wrong_content_type = False
is_null_content = False

request_method = os.environ.get('REQUEST_METHOD', None)
host = os.environ.get('HTTP_HOST', None)
cookie = os.environ.get('HTTP_COOKIE', None)
content_type = os.environ.get('CONTENT_TYPE', None)
uri = os.environ.get('REQUEST_URI', None)

if request_method:
    token_signing_file.write('REQUEST_METHOD: {}\n'.format(request_method))

if host:
    token_signing_file.write('HTTP_HOST: {}\n'.format(host))

if cookie:
    token_signing_file.write('Cookies in token signing request: {}\n'.format(cookie))
    cookies_found = True

if content_type:
    token_signing_file.write('Content type: {}\n'.format(content_type))

if uri:
    position_of_dummy = uri.find('?dummy=')
    if position_of_dummy == -1:
        output_url = uri
    else:
        output_url = uri[0:position_of_dummy]
    token_signing_file.write('REQUEST_URI: {}\n'.format(output_url))

    position_of_dummy = uri.find('&last=')
    if position_of_dummy != -1:
        is_last_request = True

    position_of_dummy = uri.find('&wrongContentType=')
    if position_of_dummy != -1:
        is_wrong_content_type = True

    position_of_dummy = uri.find('&nullContent=')
    if position_of_dummy != -1:
        is_null_content = True

if not cookies_found:
    token_signing_file.write('No cookies in token signing request.\n')

request_body = ''.join(sys.stdin.readlines())
token_signing_file.write('Request body:\n{}\n'.format(request_body))
token_signing_file.close()

if is_last_request:
    if os.path.isfile('{}.tmp'.format(token_signing_filepath)):
        os.rename('{}.tmp'.format(token_signing_filepath), token_signing_filepath)

content_type = 'application/json'
if is_wrong_content_type:
    content_type = 'text/html'

content = '{"token_public_key": "ABCD", "unlinkable_token": "ABCD"}'
if is_null_content:
    content = ''

sys.stdout.write(
    'status: 201\r\n'
    'Set-Cookie: cookieSetInTokenSigningResponse=1; path=/\r\n'
    'Content-Type: {}\r\n\r\n'
    '{}'.format(content_type, content)
)
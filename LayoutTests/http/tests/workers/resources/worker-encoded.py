#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

koi8 = '\xF0\xD2\xC9\xD7\xC5\xD4'
windows = '\xCF\xF0\xE8\xE2\xE5\xF2'
utf = '\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82'

charset = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('charset', [''])[0]

sys.stdout.write(
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Cache-Control: no-cache, must-revalidate\r\n'
    'Pragma: no-cache\r\n'
    'Content-Type: text/javascript\r\n\r\n'
)

content_type = 'utf-8'
if charset == 'koi8-r':
    content_type = 'koi8-r'
    sys.stdout.write('postMessage(\'Has http header with charset=koi8-r\');')
else:
    sys.stdout.write('postMessage(\'Has no http header with charset\');')

koi8 = koi8.encode('latin-1').decode(content_type, 'replace')
windows = windows.encode('latin-1').decode(content_type, 'replace')
utf = utf.encode('latin-1').decode(content_type, 'replace')

sys.stdout.write(
    'postMessage(\'Original test string: \' + String.fromCharCode(0x41F, 0x440, 0x438, 0x432, 0x435, 0x442));'
    'postMessage(\'Test string encoded using koi8-r: {}.\');'
    'postMessage(\'Test string encoded using Windows-1251: {}.\');'
    'postMessage(\'Test string encoded using UTF-8: {}.\');'.format(koi8, windows, utf)
)

# Test how XHR decodes its response text. Should be UTF8 or a charset from http header.
sys.stdout.write(
    'var xhr = new XMLHttpRequest(); xhr.open(\'GET\', \'xhr-response.py\', false);'
    'xhr.send(); postMessage(xhr.responseText);'
    'var xhr = new XMLHttpRequest(); xhr.open(\'GET\', \'xhr-response.py?charset=koi8-r\', false);'
    'xhr.send(); postMessage(xhr.responseText);'
)

# Test that URL completion is done using UTF-8, regardless of the worker's script encoding.
# The server script verifies that query parameter is encoded in UTF-8.
sys.stdout.write(
    'var xhr = new XMLHttpRequest(); xhr.open(\'GET\', \'xhr-query-utf8.py?query=\' + String.fromCharCode(0x41F, 0x440, 0x438, 0x432, 0x435, 0x442), false);'
    'xhr.send(); postMessage(xhr.responseText);'
    'importScripts(\'subworker-encoded.py\');'
    'postMessage(\'exit\');'
)
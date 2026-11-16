#!/usr/bin/env python3

import sys

koi8 = '\xF0\xD2\xC9\xD7\xC5\xD4'.encode('latin-1').decode('utf-8', 'replace')
windows = '\xCF\xF0\xE8\xE2\xE5\xF2'.encode('latin-1').decode('utf-8', 'replace')
utf = '\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82'.encode('latin-1').decode()

sys.stdout.write(
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Cache-Control: no-cache, must-revalidate\r\n'
    'Pragma: no-cache\r\n'
    'Content-Type: text/javascript\r\n\r\n'
    'postMessage(\'Sub: Original test string: \' + String.fromCharCode(0x41F, 0x440, 0x438, 0x432, 0x435, 0x442));'
    'postMessage(\'Sub: Test string encoded using koi8-r: {}.\');'
    'postMessage(\'Sub: Test string encoded using Windows-1251: {}.\');'
    'postMessage(\'Sub: Test string encoded using UTF-8: {}.\');'.format(koi8, windows, utf)
)

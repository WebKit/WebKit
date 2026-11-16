#!/usr/bin/env python3

import sys

sys.stdout.write('Content-Disposition: attachment; filename=SU\xf3\xf3\xe5SS.txt\r\n'.encode('latin-1').decode('koi8-r', 'replace'))
sys.stdout.write(
    'Content-Type: text/plain; charset=windows-1251\r\n\r\n'
    'Test file content.'
)
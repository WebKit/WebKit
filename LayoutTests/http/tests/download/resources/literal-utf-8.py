#!/usr/bin/env python3

import sys

sys.stdout.write('Content-Disposition: attachment; filename=SU\xd0\xa1\xd0\xa1\xd0\x95SS.txt\r\n'.encode('latin-1').decode('utf-8', 'replace'))
sys.stdout.write(
    'Content-Type: text/plain; charset=koi8-r\r\n\r\n'
    'Test file content.'
)
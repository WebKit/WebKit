#!/usr/bin/env python3

import os
import sys
import tempfile
from urllib.parse import parse_qs

filename = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('filename', ['state.txt'])[0]
tmpFilePath = os.path.join(tempfile.gettempdir(), filename)

sys.stdout.write('Content-Type: text/html\r\n\r\n')

stat = os.stat(tmpFilePath)
if not stat:
    sys.stdout.write('FAIL: stat() call failed.\n')
    sys.exit(0)

atime = stat.st_atime
mtime = stat.st_mtime
try:
    os.utime(tmpFilePath, times=(atime, mtime + 1))
except:
    sys.stdout.write('FAIL: touch() call failed.\n')
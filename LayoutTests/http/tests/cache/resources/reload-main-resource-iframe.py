#!/usr/bin/env python3

import os
import sys
import tempfile
from datetime import datetime, timedelta

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))

sys.stdout.write('Content-Type: text/html\r\n\r\n')

tmpFile = os.path.join(tempfile.gettempdir(), 'cache-reload-main-resource.tmp')
if os.path.isfile(tmpFile):
    os.remove(tmpFile)
    sys.stdout.write(
        '<body>\n'
        '<script>\n'
        'window.parent.finish();\n'
        '</script>\n'
        '</body>\n'
    )
    sys.exit(0)

file = open(tmpFile, 'w+')
file.close()

max_age = 12 * 31 * 24 * 60 * 60
last_modified = '{} +0000'.format((datetime.utcnow() + timedelta(seconds=max_age)).strftime('%a, %d %b %Y %H:%M:%S'))
expires = '{} +0000'.format((datetime.utcnow() + timedelta(seconds=max_age)).strftime('%a, %d %b %Y %H:%M:%S'))

sys.stdout.write(
    f'Cache-Control: public, max-age={max_age}\r\n'
    f'Expires: {expires}\r\n'
    f'Last-Modified: {last_modified}\r\n\r\n'
)
#!/usr/bin/env python3

import os
import sys
import tempfile
from datetime import datetime, timedelta
from urllib.parse import parse_qs

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

test = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('test', ['state'])[0]
tmpFilePath = os.path.join(tempfile.gettempdir(), '{}_state'.format(test))

if os.path.isfile(tmpFilePath):
    sys.stdout.write(
        'status: 404\r\n'
        'Content-Type: text/html\r\n\r\n{}'.format(tmpFilePath)
    )
    sys.exit(0)

tmpFile = open(tmpFilePath, 'w+')
tmpFile.close()

filePath = os.path.join(os.path.dirname(__file__), 'compass-no-cache.jpg')
stat = os.stat(filePath)
filemtime = stat.st_mtime
filesize = stat.st_size

etag = '"{}-{}"'.format(filesize, filemtime)
last_modified = '{} +0000'.format(datetime.utcfromtimestamp(filemtime).strftime('%a, %d %b %Y %H:%M:%S'))
max_age = 12 * 31 * 24 * 60 * 60
expires = '{} +0000'.format((datetime.utcnow() + timedelta(seconds=max_age)).strftime('%a, %d %b %Y %H:%M:%S'))

sys.stdout.write(
    'Cache-Control: public, max-age={}\r\n'
    'Expires: {}\r\n'
    'Content-Type: image/png\r\n'
    'Content-Length: {}\r\n'
    'Etag: {}\r\n'
    'Last-Modified: {}\r\n\r\n'.format(max_age, expires, filesize, etag, last_modified)
)

sys.stdout.flush()
with open(filePath, 'rb') as file:
    sys.stdout.buffer.write(file.read())
sys.exit(0)

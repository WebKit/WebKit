#!/usr/bin/env python3

import os
import sys
import time
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
name = query.get('name', [''])[0]
stall_at = query.get('stallAt', [None])[0]
stall_for = query.get('stallFor', [None])[0]
stall_repeat = query.get('stallRepeat', [None])[0]
mime_type = query.get('mimeType', [''])[0]

if not os.path.isfile(name):
    sys.stdout.write('Content-Type: text/html\r\n\r\n')
    sys.exit(0)

name = os.path.abspath(name)

open_file = open(name, 'rb')
file_size = os.path.getsize(name)
content = open_file.read()

sys.stdout.write(
    'Content-Type: {}\r\n'
    'Content-Length: {}\r\n\r\n'.format(mime_type, file_size)
)

if stall_at is not None and stall_for is not None:
    stall_at = int(stall_at)
    stall_for = float(stall_for)

    if stall_at > file_size:
        sys.stdout.write('Incorrect value for stallAt.')
        sys.exit(0)

    written_total = 0
    while written_total < file_size:
        stall_at = min(stall_at, file_size - written_total)
        written = 0

        while written < stall_at:
            write = min(1024, stall_at - written)

            sys.stdout.flush()
            sys.stdout.buffer.write(content[written_total + written:written_total + written + write])
            sys.stdout.flush()

            written += write

        written_total += written
        remaining = file_size - written_total

        if remaining == 0:
            break

        time.sleep(stall_for)
        if stall_repeat is None:
            stall_at = remaining
else:
    sys.stdout.flush()
    sys.stdout.buffer.write(content)
    sys.stdout.flush()

open_file.close()

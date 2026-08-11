#!/usr/bin/env python3

import os
import sys
import gzip
import random
import hashlib
import string
from urllib.parse import parse_qs

# Check if client supports gzip encoding
accept_encoding = os.environ.get('HTTP_ACCEPT_ENCODING', '')
supports_gzip = 'gzip' in accept_encoding

# Parse query parameters
query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)

# size of bytes. default is 1024.
size = int(query.get('size', ['1024'])[0])

# if provided, generate deterministic random data and cacheable. default is not.
seed = query.get('seed', [None])[0]

# gzip compression or not. default is not.
compress = supports_gzip and query.get('compress', [''])[0]

# Prepare contents
if compress:
    data = bytes([42] * size)
    data = gzip.compress(data)
else:
    # Generate random data
    if seed:
        seed_int = int(hashlib.sha256(seed.encode()).hexdigest(), 16)  # Convert SHA-256 hash to int
        random.seed(seed_int)

    data = ''.join(random.choice(string.ascii_letters + string.digits) for _ in range(size))

# Output HTTP Headers
if seed:
    sys.stdout.write(f'Cache-Control: public, max-age=31536000, immutable\r\n')

if compress:
    sys.stdout.write('Content-Encoding: gzip\r\n')

sys.stdout.write('Content-Type: application/octet-stream\r\n')
sys.stdout.write(f'Content-Length: {len(data)}\r\n')

sys.stdout.write('\r\n')
sys.stdout.flush()

# Output contents
if compress:
    sys.stdout.buffer.write(data)
else:
    sys.stdout.write(data)

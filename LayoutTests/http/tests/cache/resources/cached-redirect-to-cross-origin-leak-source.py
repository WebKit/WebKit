#!/usr/bin/env python3

import os
import sys
import tempfile
from urllib.parse import parse_qs

query_string = os.environ.get('QUERY_STRING', '')
mode = parse_qs(query_string, keep_blank_values=True).get('mode', [None])[0]

source_revalidated_marker = os.path.join(tempfile.gettempdir(), 'cached-redirect-to-cross-origin-leak-source-revalidated')
target_revalidated_marker = os.path.join(tempfile.gettempdir(), 'cached-redirect-to-cross-origin-leak-target-revalidated')

if mode == 'reset':
    for marker in (source_revalidated_marker, target_revalidated_marker):
        if os.path.isfile(marker):
            os.remove(marker)
    sys.stdout.write('Content-Type: application/json\r\n\r\n{}')
    sys.exit(0)

if mode == 'report':
    sys.stdout.write(
        'Content-Type: application/json\r\n\r\n'
        '{{"sourceWasAskedToRevalidate": {}, "targetWasAskedToRevalidate": {}}}'.format(
            'true' if os.path.isfile(source_revalidated_marker) else 'false',
            'true' if os.path.isfile(target_revalidated_marker) else 'false'
        )
    )
    sys.exit(0)

if_none_match_etag = os.environ.get('HTTP_IF_NONE_MATCH', '')

if if_none_match_etag:
    open(source_revalidated_marker, 'w').close()
    sys.stdout.write(
        'status: 304\r\n'
        'Access-Control-Allow-Origin: http://127.0.0.1:8000\r\n'
        'Access-Control-Allow-Credentials: true\r\n'
        '\r\n'
    )
    sys.exit(0)

sys.stdout.write(
    'status: 302\r\n'
    'Location: http://localhost:8000/cache/resources/cached-redirect-to-cross-origin-leak-redirect.py?{}\r\n'
    'Cache-Control: max-age=3600\r\n'
    '\r\n'.format(query_string)
)

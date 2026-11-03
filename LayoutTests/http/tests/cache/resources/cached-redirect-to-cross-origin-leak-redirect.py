#!/usr/bin/env python3

import os
import sys
import tempfile

query_string = os.environ.get('QUERY_STRING', '')
if_none_match_etag = os.environ.get('HTTP_IF_NONE_MATCH', '')
etag = "etag_{}".format(query_string)

target_revalidated_marker = os.path.join(tempfile.gettempdir(), 'cached-redirect-to-cross-origin-leak-target-revalidated')

if if_none_match_etag:
    open(target_revalidated_marker, 'w').close()

if if_none_match_etag == etag:
    sys.stdout.write(
        'status: 304\r\n'
        'Etag: {}\r\n'
        'Cache-Control: no-cache\r\n'
        '\r\n'.format(etag)
    )
    sys.exit(0)

sys.stdout.write(
    'status: 200\r\n'
    'Etag: {}\r\n'
    'Cache-Control: no-cache\r\n'
    '\r\n'
    'cross-origin-redirect-body-should-be-inaccessible-to-source'.format(etag)
)

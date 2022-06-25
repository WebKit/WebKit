#!/usr/bin/env python3

import os
import sys

# This resource won't be cached by network layer, but will be cached by appcache.

modified_since = os.environ.get('HTTP_IF_MODIFIED_SINCE', '')
none_match = os.environ.get('HTTP_IF_NONE_MATCH', '')

sys.stdout.write(
    'Last-Modified: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Cache-Control: no-cache, no-store\r\n'
    'Pragma: no-cache\r\n'
    'Content-Type: text/plain\r\n'
)

if modified_since or none_match:
    sys.stdout.write('status: 304\r\n\r\n')
else:
    sys.stdout.write('\r\nHello, world!\n')
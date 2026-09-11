#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

# Emits genuinely repeated Set-Cookie response headers, which CFNetwork coalesces into a single
# comma-joined value by the time WebKit sees them. That coalescing is what
# CookieUtil::splitCoalescedSetCookieHeader() has to undo, so this is the only way to exercise the
# response-header repair path end to end.

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
which = query.get('case', ['none'])[0]

headers = []

if which == 'repeated':
    # Two dated cookies in the month-before-day ordering CFNetwork rejects. Both should end up
    # persistent rather than session scoped, and neither should lose its attributes.
    headers.append('a=1; Expires=Sun Jan 05 2116 00:00:00 GMT; Path=/')
    headers.append('b=2; Expires=Mon Jan 06 2116 00:00:00 GMT; Path=/')
elif which == 'commavalue':
    # A single cookie whose VALUE contains ", name=". Splitting tears it in two, and the trailing
    # fragment parses as a well-formed cookie of its own. Only "a" may exist afterwards; a cookie
    # named "synthesized" would mean the repair invented one the server never sent.
    headers.append('a=1, synthesized=2; Expires=Sun Jan 05 2116 00:00:00 GMT; Path=/')
elif which == 'attributes':
    # Every protective attribute alongside a date that needs repairing. The repair re-stores the
    # cookie through an NSHTTPCookie property dictionary, so this pins that none of them are lost.
    headers.append('secure_flags=1; Expires=Sun Jan 05 2116 00:00:00 GMT; Path=/; Secure; HttpOnly; SameSite=Strict')

sys.stdout.write('Content-Type: text/html\r\n')
for header in headers:
    sys.stdout.write('Set-Cookie: {}\r\n'.format(header))
sys.stdout.write('\r\n')

sys.stdout.write('<!DOCTYPE html>\n<html><body>set</body></html>\n')

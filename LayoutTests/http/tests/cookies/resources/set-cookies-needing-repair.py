#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

# Emits Set-Cookie headers that CFNetwork's parser mishandles. Repeated headers reach WebKit
# comma-joined, so this is the only way to exercise the response-header repair end to end.

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
which = query.get('case', ['none'])[0]

monthFirst = 'Expires=Sun Jan 05 2116 00:00:00 GMT'
flags = 'Path=/; Secure; HttpOnly; SameSite=Strict'

cases = {
    'repeated': ['a=1; {}; Path=/'.format(monthFirst), 'b=2; Expires=Mon Jan 06 2116 00:00:00 GMT; Path=/'],
    'attributes': ['secure_flags=1; {}; {}'.format(monthFirst, flags)],
    'attributes-utf8': ['secure_flags_utf8=北京; {}; {}'.format(monthFirst, flags)],
    'commavalue': ['a=1, synthesized=2; {}; Path=/'.format(monthFirst)],
    'commavalue-utf8': ['session=abc, admin=café; Path=/'],
    'wire': ['wire_value=北京x; Path=/', '名=v; Path=/'],
    'omit': ['omitted=1; {}; Path=/'.format(monthFirst), 'omitted_utf8=北京; Path=/'],
    'capped': ['capped=1; {}; Path=/'.format(monthFirst)],
    'identity-root': ['consent=1; Path=/', 'older=1; Path=/'],
    'identity-repair': ['consent=1; {}; Path=/cookies'.format(monthFirst), 'older=1; {}; Path=/; Domain=not-this-host.example'.format(monthFirst)],
}
# Third-party cookies need SameSite=None and Secure to be stored at all.
for prefix in ['blocked', 'latched', 'control']:
    cases[prefix] = ['{}_date=1; {}; Path=/; Secure; SameSite=None'.format(prefix, monthFirst), '{}_utf8=北京; Path=/; Secure; SameSite=None'.format(prefix)]

out = sys.stdout.buffer
out.write(b'Content-Type: text/html\r\n')
for header in cases.get(which, []):
    out.write(b'Set-Cookie: ' + header.encode('utf-8') + b'\r\n')
out.write(b'\r\n<!DOCTYPE html>\n<html><body>set</body></html>\n')

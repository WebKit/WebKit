#!/usr/bin/env python3

from ast import literal_eval
import os
import sys
from urllib.parse import parse_qs, unquote_plus

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
charset = query.get('charset', ['UTF8'])[0]
hash_source = query.get('hashSource', [''])[0]
script = query.get('script', [''])[0]

sys.stdout.write(
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Cache-Control: no-cache, must-revalidate\r\n'
    'Pragma: no-cache\r\n'
    'Content-Type: text/html; charset={}\r\n'
    'Content-Security-Policy: script-src \'self\' {}\r\n\r\n'.format(charset, hash_source)
)

print('''<!DOCTYPE html>
<html>
<head>
<script src="didRunInlineScriptPrologue.js"></script>
<script>{}</script> <!-- Will only execute if hash_source represents a valid hash of this script. -->
<script src="didRunInlineScriptEpilogue.js"></script>
</head>
</html>'''.format(script))

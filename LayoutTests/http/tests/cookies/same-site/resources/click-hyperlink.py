#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
href = query.get('href', [None])[0]
target = query.get('target', [None])[0]

target_attr = ''
if target is not None:
    target_attr = 'target="{}"'.format(target)

sys.stdout.write('Content-Type: text/html\r\n\r\n')

print('''<!DOCTYPE>
<html>
<head>
<script>
if (window.testRunner)
    testRunner.waitUntilDone();
</script>
</head>
<body>
<a href="{}" {}>Click</a>
<script>
internals.withUserGesture(() => {{
    document.querySelector("a").click();
}});
</script>
</body>
</html>'''.format(href, target_attr))
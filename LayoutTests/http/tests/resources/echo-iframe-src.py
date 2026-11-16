#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

src = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('src', [''])[0]

sys.stdout.write('Content-Type: text/html\r\n\r\n')

print('''<!DOCTYPE html>
<html>
<head>
<script>
if (window.testRunner) {{
    testRunner.dumpAsText();
    testRunner.dumpChildFramesAsText();
    testRunner.waitUntilDone();
}}
</script>
</head>
<body>
<iframe src="{}"></iframe>
</body>
</html>'''.format(src))

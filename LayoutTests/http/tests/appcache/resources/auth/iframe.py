#!/usr/bin/env python3

import base64
import os
import sys

username = base64.b64decode(os.environ.get('HTTP_AUTHORIZATION', ' Og==').split(' ')[1]).decode().split(':')[0]

sys.stdout.write('Content-Type: text/html\r\n')

if not username:
    sys.stdout.write(
        'WWW-Authenticate: Basic realm="WebKit AppCache Test Realm"\r\n'
        'status: 401\r\n\r\n'
        'Authentication canceled'
    )
    sys.exit(0)

print('''\r\n<html manifest='manifest.py'>
<p>Subframe, should disappear.</p>
<script>
    applicationCache.oncached = parent.success;
    applicationCache.onnoupdate = parent.success;
</script>
</html>''')
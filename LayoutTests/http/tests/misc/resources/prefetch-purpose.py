#!/usr/bin/env python3

import os
import sys
from datetime import datetime, timedelta

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies

sys.stdout.write(
    'Set-Cookie: Purpose={}\r\n'
    'Content-Type: text/html\r\n'.format(os.environ.get('HTTP_PURPOSE', 'deleted'))
)

cookies = get_cookies()
purpose = cookies.get('Purpose', None)

if purpose is not None:
    expires= datetime.utcnow() - timedelta(seconds=3600)
    sys.stdout.write(
        'Set-Cookie: Purpose=deleted, expires={} GMT; Max-Age=0\r\n\r\n'
        '<h1>The cookie was set!</h1>'
        '<p>Purpose: {}'.format(expires.strftime('%a, %d-%b-%Y %H:%M:%S'), purpose)
    )
else:
    sys.stdout.write('<h1>BAD BROWSER NO COOKIE</h1>')

print('''<script>
testRunner.notifyDone();
</script>

<p>This test verifies that prefetches are sent with the HTTP request header
<b>Purpose: prefetch</b>.  To do this, the root page has a prefetch
link targetting this subresource which contains a PHP script
(resources/prefetch-purpose.py).  That initial prefetch of this
resource sets a cookie.  Later, the root page sets window.location to
target this script, which verifies the presence of the cookie, and
generates the happy test output that you hopefully see right now.''')
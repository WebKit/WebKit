#!/usr/bin/env python3

import os
import sys
from datetime import datetime, timedelta, timezone
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
name = query.get('name', [''])[0]
value = query.get('value', [''])[0]
message = query.get('message', [''])[0]
exp_time = datetime.utcnow() + timedelta(days=30)

sys.stdout.write(
    'Set-Cookie: {}={}; expires={} GMT; Max-Age={}; path=/\r\n'
    'Content-Type: text/html\r\n\r\n'
    '{}<br>'.format(name, value, exp_time.strftime('%a, %d-%b-%Y %H:%M:%S'), 60*60*24*30, message)
)

print('''<script>
if (document.location.hash) {
    setTimeout("document.location.href = document.location.hash.substring(1)", 10);
}
</script>''')
#!/usr/bin/env python3

import os
import sys
import time
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''))
noVarySearch = query.get('nvs', [''])[0]
delay = float(query.get('delay', ['0'])[0])

# A navigation that joined the prefetch shows the purpose of the prefetch request, since it was never sent itself.
purpose = os.environ.get('HTTP_SEC_PURPOSE') or os.environ.get('HTTP_PURPOSE') or 'none'

# Hold the response so the prefetch is still in flight, and its headers unknown, when the page navigates.
time.sleep(delay)

if 'redir' in query:
    # Redirect without a No-Vary-Search header, to show a redirected prefetch is checked against the first response.
    sys.stdout.write('Status: 302 Found\r\nLocation: /cache/resources/no-vary-search-delayed-prefetch.py?final=1\r\nCache-Control: max-age=60\r\n\r\n')
    sys.exit(0)

sys.stdout.write('Content-Type: text/html\r\nCache-Control: max-age=60\r\n')
if noVarySearch:
    sys.stdout.write('No-Vary-Search: {}\r\n'.format(noVarySearch))
sys.stdout.write('\r\n')

sys.stdout.write('''<body>
<p>Purpose: {}</p>
<p id="url"></p>
<script>
// The document has to be for the navigated URL, not the URL of the prefetch it was served from.
document.getElementById("url").textContent = "URL: " + location.pathname + location.search;
if (window.testRunner)
    testRunner.notifyDone();
</script>
</body>'''.format(purpose))

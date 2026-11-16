#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''))
noVarySearch = query.get('nvs', [''])[0]

# A navigation served from the prefetch cache never reaches the server, so the purpose
# reported by the page tells us which request produced the response it is showing.
purpose = os.environ.get('HTTP_PURPOSE') or os.environ.get('HTTP_SEC_PURPOSE') or 'none'

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

#!/usr/bin/env python3

import json
import os
import sys

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies

cookies = get_cookies()

sys.stdout.write('Content-Type: text/html\r\n\r\n')
print(sys.path)

print('''<!doctype html>
<script>
var from_http = {};

window.opener.postMessage({{
    'http': from_http,
    'document': document.cookie
}}, "*");
</script>'''.format(json.dumps(cookies)))
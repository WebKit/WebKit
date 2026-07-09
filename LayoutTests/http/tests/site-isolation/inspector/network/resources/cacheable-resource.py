#!/usr/bin/env python3

import sys

# A cacheable resource: with a long max-age, a repeated same-URL load is served from the cache unless
# the inspector has disabled resource caching for the page.
sys.stdout.write('Content-Type: text/plain\r\n')
sys.stdout.write('Cache-Control: max-age=3600\r\n')
sys.stdout.write('Access-Control-Allow-Origin: *\r\n')
sys.stdout.write('\r\n')
sys.stdout.write('cacheable resource body')

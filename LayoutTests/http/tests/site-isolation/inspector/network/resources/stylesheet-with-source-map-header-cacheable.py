#!/usr/bin/env python3

import sys

# Cacheable (no "no-store") so the second load in the test is served from the
# in-memory cache, exercising the RequestServedFromMemoryCache path.
sys.stdout.write('Content-Type: text/css\r\n')
sys.stdout.write('Cache-Control: max-age=3600\r\n')
sys.stdout.write('SourceMap: stylesheet-header.css.map\r\n')
sys.stdout.write('\r\n')
sys.stdout.flush()

sys.stdout.write('body { color: green; }\n')

#!/usr/bin/env python3

import sys

sys.stdout.write('Content-Type: text/css\r\n')
sys.stdout.write('Cache-Control: no-store\r\n')
sys.stdout.write('SourceMap: stylesheet-header.css.map\r\n')
sys.stdout.write('\r\n')
sys.stdout.flush()

sys.stdout.write('body { color: green; }\n')

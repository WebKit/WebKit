#!/usr/bin/env python3

import sys

sys.stdout.write('Content-Type: text/plain\r\nClear-Site-Data: "prefetchCache"\r\nCache-Control: no-store\r\n\r\n')
sys.stdout.write('cleared')

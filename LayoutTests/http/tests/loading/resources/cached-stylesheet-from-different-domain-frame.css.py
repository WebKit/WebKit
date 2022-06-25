#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Cache-Control: public, max-age=264\r\n'
    'Content-Type: text/css\r\n\r\n'
    '@import "imported-stylesheet-varying-according-domain.css.py?domain=127.0.0.1:8000"'
)
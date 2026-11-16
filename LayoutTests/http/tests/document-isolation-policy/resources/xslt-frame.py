#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Type: text/xml\r\n'
    'Document-Isolation-Policy: isolate-and-require-corp\r\n\r\n'
    '<?xml version="1.0"?>\n'
    '<?xml-stylesheet type="text/xsl" href="xslt-frame.xsl"?>\n'
    '<root/>\n'
)

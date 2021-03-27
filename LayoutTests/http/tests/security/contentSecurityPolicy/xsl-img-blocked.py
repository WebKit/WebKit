#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy: img-src \'none\'\r\n'
    'Content-Type: text/xml\r\n\r\n'
    '<?xml version="1.0" encoding="UTF-8"?>\n'
    '<?xml-stylesheet type="text/xsl" href="resources/transform-to-img.xsl"?>\n'
    '<body/>\n'
)
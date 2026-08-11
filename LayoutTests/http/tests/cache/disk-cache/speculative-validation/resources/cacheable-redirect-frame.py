#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Cache-Control: max-age=0\r\n'
    'Etag: 123456789\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<head>\n'
    '<link rel="stylesheet" type="text/css" href="redirect-to-css.py">\n'
    '</head>\n'
    '<body>\n'
    '<div id="testDiv" class="testClass">\n'
    'TEST\n'
    '</div>\n'
    '</body>\n'
    '</html>\n'
)
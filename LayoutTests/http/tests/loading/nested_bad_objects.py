#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)

if 'object' in query.keys():
    sys.stdout.write('Content-Type: nothing/bad-type\r\n\r\n')
else:
    sys.stdout.write(
        'Content-Type: text/html\r\n\r\n'
        '<html>\n'
        '    <script>\n'
        '    if (window.testRunner)\n'
        '        testRunner.dumpAsText();\n'
        '    </script>\n'
        '    <object type="image/jpeg" data="nested_bad_objects.py?object">\n'
        '        <object type="image/jpeg" data="nested_bad_objects.py?object" />\n'
        '    </object>\n'
        '    PASS - nested image objects with bad mimetype do not cause a crash.\n'
        '</html>\n'
)
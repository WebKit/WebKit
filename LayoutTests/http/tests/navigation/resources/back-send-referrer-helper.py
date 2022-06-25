#!/usr/bin/env python3

import os
import sys

referer = os.environ.get('HTTP_REFERER', '')

sys.stdout.write(
    'Cache-Control: no-cache, private, max-age=0\r\n'
    'Content-Type: text/html\r\n\r\n'
    '\n'
    '<html>\n'
    '\n'
    '<script>\n'
    '    window.name = parseInt(window.name) + 1;\n'
    '</script>\n'
    '\n'
    'Referrer: {}\n'
    '<br/>\n'
    'window.name: <script>document.write(window.name)</script>\n'
    '\n'
    '<form name=loopback action="" method=GET></form>\n'
    '\n'
    '<script>\n'
    '    if (window.name == 1) {{\n'
    '        // Navigate once more (in a timeout) to add a history entry.\n'
    '        setTimeout(function() {{document.loopback.submit();}}, 0);\n'
    '    }} else if (window.name == 2) {{\n'
    '        history.go(-1);\n'
    '    }} else {{\n'
    '        if (window.testRunner)\n'
    '            window.testRunner.notifyDone();\n'
    '    }}\n'
    '</script>\n'
    '\n'
    '</html>\n'.format(referer)
)
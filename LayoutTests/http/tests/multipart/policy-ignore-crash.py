#!/usr/bin/env python3

import sys

sys.stdout.buffer.write(
    'Content-Type: multipart/x-mixed-replace;boundary=asdf\r\n\r\n'
    '--asdf\n'
    'Content-type: text/html\n'
    '\n'
    '<p>This test passes if it does not crash.</p>\n'
    '<script>\n'
    'if (window.testRunner)\n'
    '    testRunner.dumpAsText();\n'
    '</script>\n'
    '{}\r\n'.format(' ' * 5000).encode()
)

sys.stdout.flush()

sys.stdout.buffer.write(
    '--asdf\n'
    'Content-type: text/rtf\n'
    '\n'
    'This chunk has an unsupported text mime type, which can cause the policy\n'
    'for this load to be ignored. This causes the request to be canceled.\n'
    '\n'
    '{}\r\n'
    '--asdf--\n'.format(' ' * 5000).encode()
)

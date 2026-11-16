#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy: sandbox allow-scripts allow-modals\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<script>\n'
    'console.log(\'PASS (1/2): Script can execute\');\n'
    '</script>\n'
    '<script>\n'
    'eval("console.log(\'PASS (2/2): Eval works\')");\n'
    '</script>\n'
    'Done.\n'
)
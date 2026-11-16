#!/usr/bin/env python3

import os
import sys

referer = os.environ.get('HTTP_REFERER', '')
sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<script>\n'
    'if (window.parent.testRunner)\n'
    '    testRunner.notifyDone();\n'
    '</script>\n'
    '{}'.format(referer)
)
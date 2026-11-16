#!/usr/bin/env python3

import sys
import time

sys.stdout.write(
    'status: 200\r\n'
    'Content-Type: text/html\r\n\r\n'
)

sys.stdout.flush()
sys.stdout.write('{}<pre>'.format(' ' * 1024))
sys.stdout.flush()

sys.stdout.write(
    '<script>\n'
    '\n'
    'if (window.testRunner) {\n'
    '    testRunner.dumpAsText();\n'
    '    testRunner.waitUntilDone();\n'
    '}\n'
    '\n'
    '</script>\n'
    '\n'
    'This page takes forever to load.<br>\n'
    'It also blocks on an external script that takes forever to load.<br>\n'
    'Without the fix for http://webkit.org/b/117278, navigating away from this page will crash.<br>\n'
    'So... navigating away should not crash!<br>\n'
    '\n'
    '<script>\n'
    '\n'
    'setTimeout("location.href = \'resources/notify-done.html\'", 0);\n'
    '\n'
    '</script>\n'
    '\n'
    '<script src=\'resources/slow-loading-script.py\'></script>\n'
)

while True:
    sys.stdout.write('Still loading...<br>\r\n')
    sys.stdout.flush()
    time.sleep(1)
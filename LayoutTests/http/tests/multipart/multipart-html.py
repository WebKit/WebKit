#!/usr/bin/env python3

import sys
import time

padding = ' ' * 5000

sys.stdout.write(
    'Connection: keep-alive\r\n'
    'Content-type: multipart/x-mixed-replace; boundary=boundary\r\n\r\n'
    '--boundary\r\n'
    'Content-Type: text/html\r\n\r\n'
    f'{padding}\n'
    '\n'
    '<script>\n'
    'if (window.testRunner) {\n'
    '    testRunner.dumpAsText();\n'
    '    if (testRunner.setShouldDecideResponsePolicyAfterDelay)\n'
    '        testRunner.setShouldDecideResponsePolicyAfterDelay(true);\n'
    '}\n'
    '</script>\n\n'
)

for i in range(0, 11):
    sys.stdout.write(
        '--boundary\r\n'
        'Content-Type: text/html\r\n\r\n'
    )
    
    if i < 10:
        sys.stdout.write(
            'This is message {}.<br>'
            'FAIL: The message number should be 10.'.format(i)
        )
    else:
        sys.stdout.write('PASS: This is message {}.'.format(i))

    sys.stdout.write('{}\r\n\r\n'.format(padding))
    sys.stdout.flush()
    time.sleep(0.1)
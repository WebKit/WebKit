#!/usr/bin/env python3

import sys
import time

padding = ' ' * 5000

sys.stdout.buffer.write(
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
    '</script>\n\n'.encode()
)

for i in range(0, 11):
    sys.stdout.buffer.write(
        b'--boundary\r\n'
        b'Content-Type: text/html\r\n\r\n'
    )
    
    if i < 10:
        sys.stdout.buffer.write(
            'This is message {}.<br>'
            'FAIL: The message number should be 10.'.format(i).encode()
        )
    else:
        sys.stdout.buffer.write('PASS: This is message {}.'.format(i).encode())

    sys.stdout.buffer.write('{}\r\n\r\n'.format(padding).encode())
    sys.stdout.flush()
    time.sleep(0.1)

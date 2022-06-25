#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

test = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('test', [''])[0]

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    'if (window.testRunner) {{\n'
    '    testRunner.dumpAsText();\n'
    '    testRunner.waitUntilDone();\n'
    '}}\n'
    '\n'
    'window.onload = function () {{\n'
    '    window.location = "/security/contentSecurityPolicy/resources/echo-report.py?test={}";\n'
    '}}\n'.format(test)
)
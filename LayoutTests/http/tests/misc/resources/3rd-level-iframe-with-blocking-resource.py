#!/usr/bin/env python3

import sys
import time

time.sleep(1)

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<html>\n'
    '<head>\n'
    '    <script src=\'notfound.js\'></script>\n'
    '    <style>body { background-color: red; }</style>\n'
    '</head>\n'
    '<body>\n'
    '    PASS.\n'
    '    <script>\n'
    '        if (window.testRunner) {\n'
    '            testRunner.notifyDone();\n'
    '        }\n'
    '    </script>\n'
    '</body>\n'
    '</html>\n'
)
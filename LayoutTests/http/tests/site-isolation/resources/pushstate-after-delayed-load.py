#!/usr/bin/env python3

import sys
import time

# Keep the load provisional while the frame's previous process calls document.open().
time.sleep(1)

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Cache-Control: no-store\r\n'
    '\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<body>\n'
    '<script>\n'
    'history.pushState({ }, "", "?pushed");\n'
    'parent.postMessage("PASS if the web process was not terminated. location.search is " + location.search, "*");\n'
    '</script>\n'
    '</body>\n'
    '</html>\n'
)

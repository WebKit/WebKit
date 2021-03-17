#!/usr/bin/env python3

import os
import sys

purpose = os.environ.get('HTTP_PURPOSE', '')

sys.stdout.write(
    'Cache-Control: max-age=3600\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<body>\n'
    '<script>\n'
    '\n'
    'if (window.testRunner) {\n'
    '  fetch(\'http://localhost:8000/cache/resources/prefetched-main-resource.py\').then(function(response) {\n'
    '    if (internals.fetchResponseSource(response) != "Disk cache") {\n'
    '      document.getElementById(\'log\').innerText = \'FAIL: resource is not in the disk cache.\';\n'
    '    }\n'
    '    testRunner.notifyDone();\n'
    '  });\n'
    '}\n'
    '\n'
    '</script>\n'
    '<div id="log">'
)

if purpose == 'prefetch':
    sys.stdout.write('PASS')
else:
    sys.stdout.write('FAIL')

sys.stdout.write(
    '</div>\n'
    '</body>\n'
    '</html>\n'
)
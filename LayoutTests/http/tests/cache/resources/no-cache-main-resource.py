#!/usr/bin/env python3

import sys
from random import randint

sys.stdout.write(
    'Cache-control: no-cache, max-age=0\r\n'
    'Last-Modified: Thu, 01 Jan 1970 00:00:00 GMT\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<script>\n'
    f'var randomNumber = {randint(0, sys.maxsize + 1)};\n'
    'opener.postMessage(randomNumber, \'*\');\n'
    '\n'
    '// Our opener will tell us to perform various loads.\n'
    'window.addEventListener(\'message\', function(event) {\n'
    '\n'
    '    // Go forward and back.\n'
    '    if (event.data === \'go-forward-and-back\') {\n'
    '        window.location = \'history-back.html\';\n'
    '        return;\n'
    '    }\n'
    '\n'
    '    // Reload.\n'
    '    if (event.data === \'reload\') {\n'
    '        window.location.reload();\n'
    '        return;\n'
    '    }\n'
    '\n'
    '    // Normal navigation, next.\n'
    '    if (event.data === \'next\') {\n'
    '        window.location = \'no-cache-main-resource-next.py\';\n'
    '        return;\n'
    '    }\n'
    '\n'
    '}, false);\n'
    '</script>\n'
)
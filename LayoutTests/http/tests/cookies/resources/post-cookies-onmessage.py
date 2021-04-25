#!/usr/bin/env python3

import json
import os
import sys

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies

cookies = get_cookies()

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<!doctype html>\n'
    '<script>\n'
    '\n'
    'var from_http = {};\n'
    '\n'
    'window.addEventListener("message", e => {{\n'
    '    e.source.postMessage({{\n'
    '        \'http\': from_http,\n'
    '        \'document\': document.cookie\n'
    '    }}, "*");\n'
    '}});\n'
    '</script>\n'.format(json.dumps(cookies))
)
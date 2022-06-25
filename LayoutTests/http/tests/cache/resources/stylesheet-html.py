#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

sheet = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('sheet', ['404.css'])[0]

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    f'<link rel=stylesheet href={sheet}>'
    '<div id=test1></div>\n'
    '<div id=test2></div>\n'
)
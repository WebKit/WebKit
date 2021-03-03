#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

charset = query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('charset', [''])[0]

sys.stdout.write(
    'Content-Type: text/html; charset=utf-8\r\n\r\n'
    '<meta charset="{charset}">\n'
    '<body onload="top.frameLoaded()">\n'
    '<p id=charset>{charset}</p>\n'
    '<p id=test>{text}</p>\n'
    '</body>\n'.format(charset=charset, text='SUóóåSS'.encode('ISO-8859-1').decode('KOI8-R'))
)
#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs, urlencode

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
filename = query.get('name', [''])[0]
typ = query.get('type', [''])[0]
ranges = query.get('ranges', [None])[0]

query_obj = {'name': filename, 'type': typ}
if ranges is not None:
    query_obj.update({'ranges': ranges})

os.environ['QUERY_STRING'] = urlencode(query_obj)

from serve_video import serve_video

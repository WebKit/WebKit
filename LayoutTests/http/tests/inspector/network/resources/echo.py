#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
mimeType = query.get('mimetype', ['text/plain'])[0]
content = query.get('content', ['Missing mimetype or content query parameter.'])[0]

sys.stdout.write('Content-Type: {}\r\n\r\n'.format(mimeType))
sys.stdout.write(content)
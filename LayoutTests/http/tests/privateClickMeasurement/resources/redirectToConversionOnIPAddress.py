#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
conversion_data = query.get('conversionData', [None])[0]
priority = query.get('priority', [None])[0]

sys.stdout.write(
    'status: 302\r\n'
    'Content-Type: text/html\r\n'
)

if conversion_data is not None and priority is not None:
    sys.stdout.write('Location: https://127.0.0.1:8000/.well-known/private-click-measurement/trigger-attribution/{}/{}\r\n\r\n'.format(conversion_data, priority))
elif conversion_data is not None:
    sys.stdout.write('Location: https://127.0.0.1:8000/.well-known/private-click-measurement/trigger-attribution/{}\r\n\r\n'.format(conversion_data))
#!/usr/bin/env python3

import os
import sys
import time
from urllib.parse import parse_qs

sys.stdout.write('Content-Type: text/html\r\n')

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
delay = int(query.get('delay', [100])[0])
redirect = query.get('redirect', ['redirect.py'])[0]

# php usleep is in microseconds
# python time.sleep is in seconds
time.sleep(delay / 1000)

sys.stdout.write('Location: {}\r\n\r\n'.format(redirect))
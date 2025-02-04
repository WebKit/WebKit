#!/usr/bin/env python3

import os
import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion
import random
import string
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
size = int(query.get('size', ['0'])[0])

sys.stdout.write('Content-Type: application/octet-stream\r\n')
sys.stdout.write(f'Content-Length: {size}\r\n')
sys.stdout.write('\r\n')

random_string = ''.join(random.choice(string.ascii_letters + string.digits) for _ in range(size))
sys.stdout.write(random_string)

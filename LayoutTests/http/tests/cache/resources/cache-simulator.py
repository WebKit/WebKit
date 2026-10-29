#!/usr/bin/env python3

import os
import sys
from random import randint
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
random_number = randint(0, 999999999999)

sys.stdout.write('Content-Type: text/javascript\r\n')
for name, values in query.items():
    if name != 'uniqueId':
        sys.stdout.write(f'{name}: {values[0]}\r\n')

sys.stdout.write(
    f'ETag: {random_number}\r\n\r\n'
    f'randomNumber = {random_number};\n'
)

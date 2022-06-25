#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
destination_origin = query.get('destinationOrigin', [''])[0]
is_testing_multipart = query.get('isTestingMultipart', [None])[0]
value = query.get('value', [''])[0]
id = query.get('id', [''])[0]

sys.stdout.write('Referrer-Policy: {}\r\n'.format(value))
if is_testing_multipart is not None and is_testing_multipart == '1':
    sys.stdout.write(
        'Content-Type: multipart/x-mixed-replace;boundary=boundary\r\n\r\n'
        '--boundary\r\n'
        'Referrer-Policy: {}\r\n'
        'Content-type: text/html\r\n'
        '\r\n'
        '<iframe src=\'{}security/resources/postReferrer.py?id={}\'></iframe>\r\n'
        '--boundary\r\n'.format(value, destination_origin, id)
    )
else:
    sys.stdout.write(
        'Content-Type: text/html\r\n\r\n'
        '\r\n'
        '<iframe src=\'{}security/resources/postReferrer.py?id={}\'></iframe>\r\n'.format(destination_origin, id)
    )

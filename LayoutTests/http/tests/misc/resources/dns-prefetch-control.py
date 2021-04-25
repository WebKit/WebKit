#!/usr/bin/env python3

import os
import random
import sys
from urllib.parse import parse_qs

value = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('value', [''])[0]

sys.stdout.write('Content-Type: text/html\r\n')

if value in ['off', 'on', 'foo']:
    sys.stdout.write('X-DNS-Prefetch-Control: {}\r\n\r\n'.format(value))
else:
    sys.stdout.write('\r\n')

fake_domain = 'www.{}.invalid'.format(random.randint(0, 32768))
sys.stdout.write(
    '<html>\n'
    '<body>\n'
    '<a href=\'http://{domain}/\'>{domain}</a>\n'
    '</body>\n'
    '</html>\n'.format(domain=fake_domain)
)
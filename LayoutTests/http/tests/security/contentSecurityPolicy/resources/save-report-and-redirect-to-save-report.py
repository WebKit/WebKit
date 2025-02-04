#!/usr/bin/env python3

import os
import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion
from save_report import save_report

query_string = os.environ.get('QUERY_STRING', '')
if query_string != '':
    query_string = '?{}'.format(query_string)

sys.stdout.write(
    'status: 307\r\n'
    'Location: save-report.py{}\r\n'.format(query_string)
)

save_report(True)

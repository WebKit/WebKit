#!/usr/bin/env python3

import os
import sys
import tempfile
from urllib.parse import parse_qs

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

recipient = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('recipient', [None])[0]
nonce = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('nonce', [None])[0]

if recipient is not None:
    conversion_file_name = 'privateClickMeasurementConversion{}'.format(recipient)
else:
    conversion_file_name = 'privateClickMeasurementConversion'

if nonce is not None:
    conversion_file_name = conversion_file_name + '{}.txt'.format(nonce)
else:
    conversion_file_name = conversion_file_name + '.txt'

conversion_file_path = os.path.join(tempfile.gettempdir(), conversion_file_name)

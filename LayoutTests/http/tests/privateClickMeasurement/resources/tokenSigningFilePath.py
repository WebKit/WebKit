#!/usr/bin/env python3

import os
import sys
import tempfile
from urllib.parse import parse_qs

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

dummy = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('dummy', [''])[0]

token_signing_filename = 'privateClickMeasurementTokenSigningRequest{}.txt'.format(dummy)
token_signing_filepath = os.path.join(tempfile.gettempdir(), token_signing_filename)
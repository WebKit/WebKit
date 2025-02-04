#!/usr/bin/env python3
import os
import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion
import tempfile
from urllib.parse import parse_qs

filename = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('filename', ['state.txt'])[0]
state_file = os.path.join(tempfile.gettempdir(), filename)

sys.stdout.write('Content-Type: text/html\r\n\r\n')

if os.path.exists(state_file):
    os.remove(state_file)

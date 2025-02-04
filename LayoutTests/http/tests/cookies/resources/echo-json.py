#!/usr/bin/env python3

import json
import os
import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies

cookies = get_cookies()

sys.stdout.write(
    'Content-Type: application/json\r\n'
    'Access-Control-Allow-Credentials: true\r\n'
    'Access-Control-Allow-External: true\r\n'
    'Access-Control-Allow-Origin: {}\r\n\r\n{}'.format(os.environ.get('HTTP_ORIGIN', ''), json.dumps(cookies))
)

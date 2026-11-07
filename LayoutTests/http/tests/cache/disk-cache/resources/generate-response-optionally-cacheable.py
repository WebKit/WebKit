#!/usr/bin/env python3

import os
import sys
import time
from email.utils import formatdate

sys.stdout.write('Status: 200\r\n')
if os.environ.get('HTTP_X_CACHEABLE') == 'true':
    sys.stdout.write(f'Expires: {formatdate(time.time() + 100, usegmt=True)}\r\n')
else:
    sys.stdout.write('Cache-Control: no-store\r\n')
sys.stdout.write('\r\n')

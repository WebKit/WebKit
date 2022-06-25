#!/usr/bin/env python3

import os
import sys
import tempfile

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))

tmpFile = os.path.join(tempfile.gettempdir(), 'cache-partitioned-cache-state')
file = open(tmpFile, 'r')

sys.stdout.write(
    'Content-Type: application/javascript\r\n'
    'Cache-Control: max-age=3600\r\n\r\n'
    f'var response = \'{file.read()}\';\n'
)

file.close()
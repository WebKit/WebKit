#!/usr/bin/env python3

import os
import sys

sys.stdout.write('Content-Type: text/html\r\n\r\n{}'.format(os.environ.get('HTTP_COOKIE', '')))
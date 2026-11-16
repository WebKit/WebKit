#!/usr/bin/env python3

import os
import sys

user_agent = os.environ.get('HTTP_USER_AGENT', '')

sys.stdout.write(
    'Content-Type: application/javascript\r\n\r\n'
    'window.subresourceUserAgent = "{}";\n'.format(user_agent)
)
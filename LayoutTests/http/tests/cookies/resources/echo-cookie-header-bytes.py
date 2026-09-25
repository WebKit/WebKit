#!/usr/bin/env python3

import os
import sys

# Returns the Cookie request header exactly as received, hex encoded, since that is the only
# place the bytes a server gets back are observable.
sys.stdout.buffer.write(b'Content-Type: text/plain\r\nCache-Control: no-store\r\n\r\n' + os.environb.get(b'HTTP_COOKIE', b'').hex().encode('ascii'))

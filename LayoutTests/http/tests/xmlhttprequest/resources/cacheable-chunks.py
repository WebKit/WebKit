#!/usr/bin/env python3
import random
import sys
import time

# A cacheable response whose body is written in several separately flushed
# chunks, so that the FragmentedSharedBuffer kept by the CachedRawResource ends
# up with more than one segment.
sys.stdout.write(
    'Content-Type: text/plain\r\n'
    'Cache-Control: max-age=3600\r\n'
    'Content-Type: application/x-no-buffering-please\r\n'
    '\r\n'
)
sys.stdout.flush()

# The nonce changes on every request, so a test can tell a memory cache hit
# (same nonce as the first load) from a second trip to the network.
sys.stdout.write('nonce=%d\n' % random.getrandbits(48))
sys.stdout.flush()

for i in range(3):
    time.sleep(0.15)
    sys.stdout.write('chunk %d\n' % i)
    sys.stdout.flush()

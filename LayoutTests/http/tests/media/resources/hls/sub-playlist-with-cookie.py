#!/usr/bin/env python3

# See the HLS ITEF spec: <http://tools.ietf.org/id/draft-pantos-http-live-streaming-12.txt>

import os
import sys
from datetime import datetime

sys.stdout.write(
    'status: 200\r\n'
    'Connection: close\r\n'
    'Cache-Control: no-store, no-cache="set-cookie"\\n\r\n'
    'Access-Control-Allow-Origin: *\r\n'
    'Pragma: no-cache\r\n'
    'Etag: "{size}-{mtime}"\r\n'
    'Set-Cookie: TEST=test.ts; path=/media/resources/\r\n'
    'Content-Type: application/x-mpegurl\r\n\r\n'.format(size=os.path.getsize(__file__), mtime=str(os.stat(__file__).st_mtime).split('.')[0])
)

chunk_duration = 6.0272
chunk_count = 4

sys.stdout.write(
    '#EXTM3U\n'
    '#EXT-X-TARGETDURATION:7\n'
    '#EXT-X-VERSION:4\n'
    '#EXT-X-MEDIA-SEQUENCE:{}\n'.format(int(datetime.utcnow().timestamp() / chunk_duration) % 100)
)

time = datetime.utcnow()
time = time.timestamp() - time.timestamp() % chunk_duration

for _ in range(0, chunk_count):
    sys.stdout.write(
        '#EXT-X-PROGRAM-DATE-TIME:{}T{}+00:00\n'
        '#EXTINF:{},\n'
        '../video-cookie-check-cookie.py\n'.format(datetime.fromtimestamp(time).strftime('%Y-%m-%d'), datetime.fromtimestamp(time).strftime('%H:%M:%S'), chunk_duration)
    )

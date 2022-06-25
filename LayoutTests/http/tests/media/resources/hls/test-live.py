#!/usr/bin/env python3

# See the HLS ITEF spec: <http://tools.ietf.org/id/draft-pantos-http-live-streaming-12.txt>

import os
import sys
from datetime import datetime
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
last_modified = datetime.utcnow()

sys.stdout.write(
    'status: 200\r\n'
    'Connection: close\r\n'
    'Last-Modified: {modified} GMT\r\n'
    'Pragma: no-cache\r\n'
    'Etag: "{size}-{mtime}"\r\n'
    'Content-Type: application/x-mpegurl\r\n\r\n'.format(modified=last_modified.strftime('%a, %d %b %Y %H:%M:%S'), size=os.path.getsize(__file__), mtime=str(os.stat(__file__).st_mtime).split('.')[0])
)

chunk_duration = 6.0272
if 'duration' in query.keys():
    chunk_duration = float(query.get('duration'))

chunk_count = 4
if 'count' in query.keys():
    chunk_count = int(query.get('count'))

chunk_url = 'test.ts'
if 'url' in query.keys():
    chunk_url = query.get('url')

sys.stdout.write(
    '#EXTM3U\n'
    '#EXT-X-TARGETDURATION:7\n'
    '#EXT-X-VERSION:4\n'
    '#EXT-X-MEDIA-SEQUENCE:{}\n'.format(int(datetime.utcnow().timestamp() / chunk_duration) % 100)
)

time = datetime.utcnow()
time = time.timestamp() - time.timestamp() % chunk_duration

for _ in range(0, chunk_count):
    time += chunk_duration
    sys.stdout.write(
        '#EXT-X-PROGRAM-DATE-TIME:{}T{}+00:00\n'
        '#EXTINF:{},\n'
        '{}\n'.format(datetime.fromtimestamp(time).strftime('%Y-%m-%d'), datetime.fromtimestamp(time).strftime('%H:%M:%S'), chunk_duration, chunk_url)
    )

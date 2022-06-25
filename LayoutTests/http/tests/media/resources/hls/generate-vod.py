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
    'Content-Type: application/x-mpegurl\r\n\r\n'.format(modified=last_modified.strftime('%a, %d %b %Y %H:%M:%S'), size=os.path.getsize(__file__), mtime=os.stat(__file__).st_mtime)
)

chunk_duration = 6.0272
chunk_count = 5
if 'duration' in query.keys():
    chunk_count = int(int(query.get('duration')[0]) / chunk_duration)

sys.stdout.write(
    '#EXTM3U\n'
    '#EXT-X-TARGETDURATION:7\n'
    '#EXT-X-VERSION:4\n'
    '#EXT-X-MEDIA-SEQCE:0\n'
    '#EXT-X-PLAYLIST-TYPE:VOD\n'
)

for _ in range(0, chunk_count):
    sys.stdout.write(
        '#EXTINF:{},\n'
        'test.ts\n'.format(chunk_duration)
    )

sys.stdout.write('#EXT-X-ENDLIST\n')

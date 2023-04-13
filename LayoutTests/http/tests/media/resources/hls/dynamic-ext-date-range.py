#!/usr/bin/env python3

# See the HLS ITEF spec: <http://tools.ietf.org/id/draft-pantos-http-live-streaming-12.txt>

import os
import sys
from datetime import datetime
from datetime import timedelta
from urllib.parse import parse_qs

sys.stdout.write(
    'status: 200\r\n'
    'Connection: close\r\n'
    'Last-Modified: {modified} GMT\r\n'
    'Pragma: no-cache\r\n'
    'Etag: "{size}-{mtime}"\r\n'
    'Content-Type: application/x-mpegurl\r\n\r\n'.format(modified=datetime.utcnow().strftime('%a, %d %b %Y %H:%M:%S'), size=os.path.getsize(__file__), mtime=os.stat(__file__).st_mtime)
)

chunk_duration = 6.0272
chunk_count = 5
chunk_time = datetime.utcnow()

sys.stdout.write(
    '#EXTM3U\n'
    '#EXT-X-PLAYLIST-TYPE:EVENT\n'
    '#EXT-X-VERSION:3\n'
    '#EXT-X-MEDIA-SEQUENCE:0\n'
    '#EXT-X-TARGETDURATION:8\n'
    '#EXT-X-PROGRAM-DATE-TIME:{}\n'
    .format(chunk_time.strftime('%Y-%m-%dT%H:%M:%S.%fZ'))
)

for chunk in range(0, chunk_count):
    sys.stdout.write(
        '#EXTINF:{},\n'
        '#EXT-X-DATERANGE:ID="cue.1.{}",START-DATE="{}",DURATION=5.000000,X-TEST-DATA="data.1.{}"\n'
        '#EXT-X-DATERANGE:ID="cue.2.{}",START-DATE="{}",DURATION=5.000000,X-TEST-DATA="data.2.{}"\n'
        '#EXT-X-DATERANGE:ID="cue.3.{}",START-DATE="{}",DURATION=5.000000,X-TEST-DATA="data.3.{}"\n'
        'test.ts\n'
        .format(chunk_duration,
                chunk, chunk_time.strftime('%Y-%m-%dT%H:%M:%S.%fZ'), chunk,
                chunk, (chunk_time + timedelta(seconds=2)).strftime('%Y-%m-%dT%H:%M:%S.%fZ'), chunk,
                chunk, (chunk_time + timedelta(seconds=3)).strftime('%Y-%m-%dT%H:%M:%S.%fZ'), chunk)
    )
    chunk_time += timedelta(seconds=5)

sys.stdout.write('#EXT-X-ENDLIST\n')

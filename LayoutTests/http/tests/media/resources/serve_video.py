#!/usr/bin/env python3

# This script is based on the work done by gadgetguru
# <david@vuistbijl.nl> at
# https://github.com/gadgetguru/PHP-Streaming-Audio and released
# under the Public Domain.

import json
import math
import os
import sys
import time
from datetime import datetime
from urllib.parse import parse_qs

https = os.environ.get('HTTPS', None)

radio_url = ''
if https is None:
    radio_url += 'http://'
else:
    radio_url += 'https://'
radio_url += '{}{}'.format(os.environ.get('HTTP_HOST', ''), os.environ.get('REQUEST_URI', ''))

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)

name = query.get('name', [''])[0]
media_directory = ''
if name != '':
    media_directory = os.path.abspath(os.path.dirname(name))
file_name = name
file_size = os.path.getsize(file_name)

file_in_db = False
file_index_in_db = -1

start = 0
end = file_size - 1

# Set Variables
settings = {
    'chunkSize': int(query.get('chunkSize', [1024 * 256])[0]),
    'databaseFile': 'metadata.json',
    'httpStatus': '500 Internal Server Error',
    'mediaDirectory': media_directory,
    'mimeType': query.get('type', [''])[0],
    'radioGenre': 'Rock',
    'radioName': 'WebKit Test Radio',
    'radioUrl': radio_url,
    'setContentLength': query.get('content-length', ['yes'])[0],
    'setIcyData': query.get('icy-data', ['no'])[0],
    'supportRanges': query.get('ranges', ['yes'])[0],
    'stallOffset': int(query.get('stallOffset', [0])[0]),
    'stallDuration': int(query.get('stallDuration', [2])[0]),
}


def answering():
    sys.stdout.write(
        'status: {}\r\n'
        'Connection: close\r\n'.format(settings['httpStatus'][0:3])
    )

    if settings['httpStatus'].startswith('500'):
        sys.stdout.write(
            'Content-Type: text/html\r\n\r\n'
            '<html><body><h1>{}</h1><p/></body></html>'.format(settings['httpStatus'])
        )
        sys.stdout.flush()
        sys.exit(0)

    last_modified = datetime.utcnow()
    sys.stdout.write(
        'Last-Modified: {} GMT\r\n'
        'Cache-Control: no-cache\r\n'
        'Etag: "{}-{}"\r\n'.format(last_modified.strftime('%a, %d %b %Y %H:%M:%S'), file_size, str(os.stat(file_name).st_mtime).split('.')[0])
    )

    if settings['setIcyData'] == 'yes':
        bit_rate = math.ceil(play_files[file_index_in_db]['bitRate'] / 1000)
        if settings['mimeType'] == '':
            settings['mimeType'] = play_files[file_index_in_db]['mimeType']

        sys.stdout.write(
            'icy-notice1: <BR>This stream requires a shoutcast/icecast compatible player.<BR>\r\n'
            'icy-notice2: WebKit Stream Test<BR>\r\n'
            'icy-name: {name}\r\n'
            'icy-genre: {genre}\r\n'
            'icy-url: {url}\r\n'
            'icy-pub: 1\r\n'
            'icy-br: {rate}\r\n'.format(name=settings['radioName'], genre=settings['radioGenre'], url=settings['radioUrl'], rate=bit_rate)
        )

    sys.stdout.write('Content-Type: {}\r\n'.format(settings['mimeType']))

    if settings['supportRanges'] != 'no':
        sys.stdout.write('Accept-Ranges: bytes\r\n')
    if content_range is not None:
        sys.stdout.write('Content-Range: bytes {}-{}/{}\r\n'.format(start, end, file_size))
    sys.stdout.write('\r\n')

    offset = start
    open_file = open(file_name, 'rb')
    content = open_file.read()

    stalled_once = False
    while offset <= end:
        read_size = min(settings['chunkSize'], (end - offset) + 1)
        stall_now = False
        if not stalled_once and settings['stallOffset'] >= offset and settings['stallOffset'] < offset + read_size:
            read_size = min(settings['chunkSize'], settings['stallOffset'] - offset)
            stall_now = True

        buff = content[offset:offset + read_size]
        read_length = len(buff)

        sys.stdout.flush()
        sys.stdout.buffer.write(buff)
        sys.stdout.flush()
        offset += read_length

        if stall_now:
            time.sleep(settings['stallDuration'])
            stalled_once = True

    open_file.close()
    sys.exit(0)


if query.get('name', [None])[0] is None:
    sys.stderr.write('You have not specified a \'name\' parameter.\n')
    answering()

if not os.path.isfile(file_name):
    sys.stderr.write('The file \'{}\' doesn\'t exist.\n'.format(file_name))
    answering()
settings['databaseFile'] = settings['mediaDirectory'] + '/' + settings['databaseFile']

if settings['setIcyData'] != 'yes' and settings['mimeType'] == '':
    sys.stderr.write('You have not specified a \'type\' parameter.\n')
    answering()

if settings['setIcyData'] == 'yes':
    if not os.path.isfile(settings['databaseFile']):
        # If the metadata database file doesn't exist it has to
        # be create previously.
        #
        # Check the instructions about how to create it from the
        # create-id3-db script file in LayoutTests/media/content.

        sys.stderr.write('The metadata database doesn\'t exist. To create one, check the script \'LayoutTests/media/content/create-id3-db\'.\n')
        answering()

    play_files = {}
    with open(settings['databaseFile'], 'r') as file:
        json_content = file.read()
        play_files = json.loads(json_content)

    for play_file in play_files:
        file_index_in_db += 1
        if os.path.basename(file_name) == play_file['fileName']:
            file_in_db = True
            break

    if not file_in_db:
        sys.stderr.write('The requested file is not in the database.\n')
        answering()

# We have everything that's needed to send the media file
if settings['stallOffset'] > file_size:
    sys.stderr.write('The \'stallOffset\' offset parameter is greater than file size ({}).\n'.format(file_size))
    answering()

content_range = None
if settings['supportRanges'] != 'no' and 'HTTP_RANGE' in os.environ.keys():
    content_range = os.environ.get('HTTP_RANGE')
if content_range is not None:
    rng = content_range[len('bytes='):].split('-')
    start = int(rng[0])
    if len(rng) > 1 and rng[1] != '':
        end = int(rng[1])
    settings['httpStatus'] = '206 Partial Content'
else:
    settings['httpStatus'] = '200 OK'

answering()

#!/usr/bin/env python3

# This script acts as a stateful proxy for retrieving files. When the state is set to
# offline, it simulates a network error with a nonsense response.

import os
import re
import sys
import tempfile
import time
from datetime import datetime
from portabilityLayer import get_state, set_state
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)

test = query.get('test', [None])[0]
command = query.get('command', [None])[0]
initial_delay_arg = query.get('initialdelay', [None])[0]
chunk_size_arg = query.get('chunksize', [None])[0]
chunk_delay_arg = query.get('chunkdelay', [None])[0]

first_byte = None
last_byte = None
initial_delay = None
chunk_size = None
chunk_delay = None

if test is None:
    sys.stdout.write(
        'status: 500\r\n'
        'Content-Type: text/html\r\n\r\n'
        'test parameter must be specified for a unique test name\n'
    )
    sys.exit(0)


def generate_no_cache_http_header():
    sys.stdout.write(
        'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
        'Cache-Control: no-cache, no-store, must-revalidate\r\n'
        'Pragma: no-cache\r\n'
    )


def content_type(path):
    return {
        'html': 'text/html',
        'manifest': 'text/cache-manifest',
        'js': 'text/javascript',
        'xml': 'application/xml',
        'xhtml': 'application/xhtml+xml',
        'svg': 'application/svg+xml',
        'xsl': 'application/xslt+xml',
        'gif': 'image/gif',
        'jpg': 'image/jpeg',
        'png': 'image/png',
        'pdf': 'application/pdf'
    }.get(path.split('.')[-1], 'text/plain')


def generate_response(path, range_start=None, range_end=None):
    state = get_state(state_file)
    if state == 'Offline':
        # Simulate a network error by replying with a nonsense response.
        sys.stdout.write(
            'status: 307\r\n'
            'Location: {}\r\n\r\n'  # Redirect to self.
            'Intentionally incorrect response.'.format(os.environ.get('REQUEST_URI', ''))
        )
    else:
        # A little security checking can't hurt.
        if '..' in path:
            sys.stdout.write('Content-Type: text/html\r\n\r\n')
            sys.exit(0)

        if len(path) > 0 and path[0] == '/':
            path = '..' + path

        if 'allow-caching' not in query.keys():
            generate_no_cache_http_header()

        if initial_delay:
            time.sleep(initial_delay / 1000)

        if os.path.isfile(path):
            sys.stdout.write('Last-Modified: {} GMT\r\n'.format(datetime.utcfromtimestamp(os.stat(path).st_mtime).strftime('%a, %d %b %Y %H:%M:%S')))

            file_len = os.path.getsize(path)
            if range_start is not None or range_end is not None:
                if range_end is None or range_end >= file_len:
                    range_end = file_len - 1

                response_length = range_end - range_start + 1

                sys.stdout.write('status: 206\r\n')
                sys.stdout.write('Content-Length: %s\r\n' % (response_length))
                sys.stdout.write('Content-Range: bytes %s-%s/%s\r\n' % (range_start, range_end, file_len))
            else:
                sys.stdout.write('Content-Length: %s\r\n' % (file_len))

            sys.stdout.write('Content-Type: {}\r\n'.format(content_type(path)))
            sys.stdout.write('\r\n')

            file_to_stdout(path, file_len, range_start, range_end)
        else:
            sys.stdout.write('status: 404\r\n\r\n')


def file_to_stdout(path, file_len, range_start=None, range_end=None):
    if chunk_size:
        chunked_file_to_stdout(path, file_len, range_start, range_end, chunk_size, chunk_delay)
        return

    if range_start is not None or range_end is not None:
        range_of_file_to_stdout(path, file_len, range_start, range_end)
        return

    with open(path, 'rb') as open_file:
        sys.stdout.flush()
        sys.stdout.buffer.write(open_file.read())


def range_of_file_to_stdout(path, file_len, range_start=None, range_end=None):
    with open(path, 'rb') as open_file:
        if range_start is not None:
            open_file.seek(range_start)

        sys.stdout.flush()
        remaining = (range_end - range_start + 1) if range_start is not None and range_end is not None else file_len

        while True:
            data = open_file.read(remaining)
            if not data:
                break

            sys.stdout.buffer.write(data)
            sys.stdout.flush()


def chunked_file_to_stdout(path, file_len, range_start, range_end, chunk_size, chunk_delay):
    with open(path, 'rb') as open_file:
        if range_start is not None:
            open_file.seek(range_start)

        sys.stdout.flush()
        remaining = (range_end - range_start + 1) if range_start is not None and range_end is not None else None

        while True:
            if remaining is not None and chunk_size > remaining:
                chunk_size = remaining

            chunk = open_file.read(chunk_size)
            if not chunk:
                break

            sys.stdout.buffer.write(chunk)
            sys.stdout.flush()

            if remaining is not None:
                remaining -= len(chunk)
                if remaining <= 0:
                    break

            if chunk_delay:
                time.sleep(chunk_delay / 1000)


BYTE_RANGE_RE = re.compile(r'bytes=(\d+)-(\d+)?$')


def parse_byte_range(byte_range):
    """Returns the two numbers in 'bytes=123-456' or throws ValueError.
    The last number or both numbers may be None.
    """
    if byte_range.strip() == '':
        return None, None

    m = BYTE_RANGE_RE.match(byte_range)
    if not m:
        raise ValueError('Invalid byte range %s' % byte_range)

    first, last = [x and int(x) for x in m.groups()]
    if last and last < first:
        raise ValueError('Invalid byte range %s' % byte_range)

    return first, last


def handle_increase_resource_count_command(path):
    resource_count_file = temp_path_base() + '-count'
    resource_count = get_state(resource_count_file)
    pieces = resource_count.split(' ')
    count = 0
    if len(pieces) == 2 and pieces[0] == path:
        count = 1 + int(pieces[1])
    else:
        count = 1

    set_state(resource_count_file, '{} {}'.format(path, count))
    generate_response(path)


def handle_reset_resource_count_command():
    resource_count_file = temp_path_base() + '-count'
    set_state(resource_count_file, '0')
    generate_no_cache_http_header()
    sys.stdout.write('status: 200\r\n\r\n')


def handle_get_resource_count_command(path):
    resource_count_file = temp_path_base() + '-count'
    resource_count = get_state(resource_count_file)
    pieces = resource_count.split(' ')
    generate_no_cache_http_header()
    sys.stdout.write('status: 200\r\n\r\n')

    if len(pieces) == 2 and pieces[0] == path:
        sys.stdout.write(str(pieces[1]))
    else:
        sys.stdout.write('0')


def handle_start_resource_requests_log():
    resource_log_file = temp_path_base() + '-log'
    set_state(resource_log_file, '')
    sys.stdout.write('Content-Type: text/html\r\n\r\n')


def handle_get_resource_requests_log():
    resource_log_file = temp_path_base() + '-log'
    generate_no_cache_http_header()
    sys.stdout.write('Content-Type: text/html\r\n\r\n')

    with open(resource_log_file, 'r') as open_file:
        sys.stdout.write(open_file.read())


def handle_log_resource_request(path):
    resource_log_file = temp_path_base() + '-log'
    new_data = '\n' + path

    with open(resource_log_file, 'a') as open_file:
        open_file.write(new_data)

    sys.stdout.write('Content-Type: text/html\r\n\r\n')


def temp_path_base():
    return os.path.join(tempfile.gettempdir(), test)


state_file = temp_path_base() + '-state'

if initial_delay_arg is not None:
    initial_delay = int(initial_delay_arg)

if chunk_size_arg is not None:
    chunk_size = int(chunk_size_arg)

if chunk_delay_arg is not None:
    chunk_delay = int(chunk_delay_arg)

if command is not None:
    if command == 'connect':
        set_state(state_file, 'Online')
        sys.stdout.write('Content-Type: text/html\r\n\r\n')
    elif command == 'disconnect':
        set_state(state_file, 'Offline')
        sys.stdout.write('Content-Type: text/html\r\n\r\n')
    elif command == 'increase-resource-count':
        handle_increase_resource_count_command(query.get('path', [''])[0])
    elif command == 'reset-resource-count':
        handle_reset_resource_count_command()
    elif command == 'get-resource-count':
        handle_get_resource_count_command(query.get('path', [''])[0])
    elif command == 'start-resource-request-log':
        handle_start_resource_requests_log()
    elif command == 'clear-resource-request-log':
        handle_start_resource_requests_log()
    elif command == 'get-resource-request-log':
        handle_get_resource_requests_log()
    elif command == 'log-resource-request':
        handle_log_resource_request(query.get('path', [''])[0])
    else:
        sys.stdout.write(
            'Content-Type: text/html\r\n\r\n'
            'Unknown command: {}\n'.format(command)
        )
        sys.exit(0)
else:
    range_request = os.environ.get('HTTP_RANGE')
    if range_request:
        try:
            first_byte, last_byte = parse_byte_range(range_request)
        except ValueError as e:
            sys.stdout.write('status: 400\r\n\r\n')

    request_path = query.get('path', [''])[0]
    generate_response(request_path, first_byte, last_byte)

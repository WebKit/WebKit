#!/usr/bin/env python3

# This script acts as a stateful proxy for retrieving files. When the state is set to
# offline, it simulates a network error with a nonsense response.

import os
import sys
import tempfile
from datetime import datetime
from portabilityLayer import get_state, set_state
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
test = query.get('test', [None])[0]
command = query.get('command', [None])[0]

if test is None:
    sys.stdout.write(
        'status: 500\r\n'
        'Content-Type: text/html\r\n\r\n'
        'test parameter must be specified for a unique test name\n'
    )
    sys.exit(0)


def temp_path_base():
    return os.path.join(tempfile.gettempdir(), test)


state_file = temp_path_base() + '-state'


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
        'png': 'image/png'
    }.get(path.split('.')[-1], 'text/plain')


def generate_response(path):
    state = get_state(state_file)
    if state == 'Offline':
        # Simulate a network error by replying with a nonsense response.
        sys.stdout.write(
            'status: 307\r\n'
            'Location: {}\r\n\r\n'  # Redirect to self.
            'Intentionally incorrect response.'.format(os.environ.get('REQUEST_URI', ''))
        )
    else:
        # A little securuty checking can't hurt.
        if '..' in path:
            sys.stdout.write('Content-Type: text/html\r\n\r\n')
            sys.exit(0)

        if len(path) > 0 and path[0] == '/':
            path = '..' + path

        if 'allow-caching' not in query.keys():
            generate_no_cache_http_header()

        if os.path.isfile(path):
            sys.stdout.write(
                'Last-Modified: {} GMT\r\n'
                'Content-Type: {}\r\n\r\n'.format(datetime.utcfromtimestamp(os.stat(path).st_mtime).strftime('%a, %d %b %Y %H:%M:%S'), content_type(path))
            )

            with open(path, 'rb') as open_file:
                sys.stdout.flush()
                sys.stdout.buffer.write(open_file.read())
        else:
            sys.stdout.write('status: 404\r\n\r\n')


def handle_increate_resource_count_command(path):
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


if command is not None:
    if command == 'connect':
        set_state(state_file, 'Online')
        sys.stdout.write('Content-Type: text/html\r\n\r\n')
    elif command == 'disconnect':
        set_state(state_file, 'Offline')
        sys.stdout.write('Content-Type: text/html\r\n\r\n')
    elif command == 'increase-resource-count':
        handle_increate_resource_count_command(query.get('path', [''])[0])
    elif command == 'reset-resource-count':
        handle_reset_resource_count_command()
    elif command == 'get-resource-count':
        handle_get_resource_count_command(query.get('path', [''])[0])
    elif command == 'start-resource-request-log':
        handle_start_resource_requests_log()
    elif command == 'lear-resource-request-log':
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
    request_path = query.get('path', [''])[0]
    generate_response(request_path)

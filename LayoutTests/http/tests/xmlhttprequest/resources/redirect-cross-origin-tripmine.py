#!/usr/bin/env python3
import os
import re
import sys
import tempfile

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import set_state, get_state
from urllib.parse import parse_qs

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Cache-Control: no-cache, no-store, must-revalidate\r\n'
    'Pragma: no-cache\r\n'
    '\r\n'
)

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
stateFile = os.path.join(tempfile.gettempdir(), 'xmlhttprequest-redirect-cross-origin-tripmine_status')

command = query.get('command', [None])[0]
if command:
    if command == 'status':
        sys.stdout.write(get_state(stateFile, default=''))
    sys.exit(0)

method = os.environ.get('REQUEST_METHOD')
contentType = os.environ.get('CONTENT_TYPE')

if method == 'OPTIONS':
    # Don't allow cross-site requests with preflight.
    sys.exit(0)

# Only allow simple cross-site requests - since we did not allow preflight, this is all we should ever get.
if method not in ['GET', 'HEAD', 'POST']:
    set_state(stateFile, 'FAIL. Non-simple method {}.'.format(method))
    sys.exit(0)

if content and not re.match(r'^application\/x\-www\-form\-urlencoded(;.+)?$', contentType) \
    and not re.match(r'^multipart\/form\-data(;.+)?$', contentType) \
    and not re.match(r'^text\/plain(;.+)?$', contentType):
    set_state(stateFile, 'FAIL. Non-simple content type: {}.'.format(contentType))

if os.environ.get('HTTP_X_WEBKIT_TEST'):
    set_state(stateFile, 'FAIL. Custom header sent with a simple request.')

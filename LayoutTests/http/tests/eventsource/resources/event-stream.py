#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Type: text/event-stream\r\n\r\n\n'
    ': a comment (ignored)\n'
    'this line will be ignored since there is no field and the line itself is not a field\n'
    'field: an unknown field that will be ignored\n'
    ':another commment\n\n'
    'data : this line will be ignored since there is a space after data\n\n'
    'data\n'
    'data:\n'
    'data\n'
    ': dispatch event with two newlines\n\n'
    'data: simple\n\n'
    'data: spanning\n'
    'data:multiple\n'
    'data\n'
    'data: lines\n'
    'data\n\n'
    'id: 1\n'
    'data: id is 1\n\n'
    'data: id is still 1\n\n'
    'id\n'
    'data: no id\n\n'
    'event: open\n'
    'data: a message event with the name "open"\n\n'
)

sys.stdout.flush()
print('da', end='')
sys.stdout.flush()

sys.stdout.write(
    'ta: a message event with the name "message"\n\n'
    'data: a line ending with crlf\r\n'
    'data: a line with a : (colon)\n'
    'data: a line ending with cr\r\n'
    'retry: 10000\n'
    ': reconnection time set to 10 seconds\n\n'
    'retry: one thousand\n'
    ': ignored invalid reconnection time value\n\n'
    'retry\n'
    ': reset to ua default\n\n\n'
)
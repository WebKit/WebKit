#!/usr/bin/env python3

import os
import sys

id = float(os.environ.get('HTTP_LAST_EVENT_ID', 0)) + 1

sys.stdout.write(
    'Content-Type: text/event-stream\r\n\r\n'
    'retry: 0\n'
    'id: {id}\n'
    'data: DATA{data}\n\n'
    'id: {id_inc}\n'
    'event: msg\n'
    'data: DATA{newlines}'.format(id=id, data=int(id), id_inc=id + 0.1, newlines='\n' * (int(id) - 1))
)
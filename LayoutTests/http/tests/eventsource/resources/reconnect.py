#!/usr/bin/env python3

import os
import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write('Content-Type: text/event-stream\r\n\r\n')

lastEventId = os.environ.get('HTTP_LAST_EVENT_ID')
if lastEventId:
    print('data: {}\n'.format(lastEventId))
else:
    print('''id: 77
retry: 300
data: hello\n
data: discarded''')

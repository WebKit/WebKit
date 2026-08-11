#!/usr/bin/env python3

import json
import os
import random
import sys
import time
from datetime import datetime

sys.stdout.write('Content-Type: text/event-stream\r\n\r\n')

counter = random.randint(1, 10)
while True:
    curDate = '{}-0500'.format(datetime.now().isoformat().split('.')[0])
    data = {'time': curDate}
    sys.stdout.write(
        'event: ping\n'
        'data: {}\n\n'.format(json.dumps(data))
    )

    counter -= 1
    if not counter:
        sys.stdout.write('data: This is a message at time {}\n\n'.format(curDate))
        counter = random.randint(1, 10)

    sys.stdout.flush()

    time.sleep(1)
#!/usr/bin/env python3

import os
import sys
import time
from ping_file_path import ping_filepath

sys.stdout.write('Content-Type: text/html\r\n\r\n')

iterations = 100
while not os.path.isfile(ping_filepath) and iterations != 0:
    time.sleep(0.01)
    iterations -= 1

if iterations == 0:
    sys.stdout.write(
        '<html><body>\n'
        'Ping not sent'
    )
else:
    sys.stdout.write(
        '<html><body>\n'
        'Ping sent successfully'
    )

    with open(ping_filepath, 'r') as ping_file:
        for line in ping_file.readlines():
            sys.stdout.write('<br>{}'.format(line.strip()))

    if os.path.isfile(ping_filepath):
        os.remove(ping_filepath)

sys.stdout.write(
    '<script>'
    'if (window.testRunner)'
    '    testRunner.notifyDone();'
    '</script>'
    '</body></html>'
)

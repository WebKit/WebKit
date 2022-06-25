#!/usr/bin/env python3

import os
import sys
import time
from ping_file_path import ping_filepath

sys.stdout.write('Content-Type: text/html\r\n\r\n')

while not os.path.isfile(ping_filepath):
    time.sleep(0.01)

sys.stdout.write(
    '<html><body>\n'
    'Ping sent successfully'
)

ping_file = open(ping_filepath, 'r')
for line in ping_file.readlines():
    sys.stdout.write('<br>{}'.format(line.strip()))

ping_file.close()
if os.path.isfile(ping_filepath):
    os.remove(ping_filepath)

sys.stdout.write(
    '<script>'
    'if (window.testRunner)'
    '    testRunner.notifyDone();'
    '</script>'
    '</body></html>'
)

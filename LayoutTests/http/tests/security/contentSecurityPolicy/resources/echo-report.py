#!/usr/bin/env python3

import os
import sys
import time
from report_file_path import report_filepath

while not os.path.isfile(report_filepath):
    time.sleep(0.01)

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<html><body>\n'
    'CSP report received:'
)

report_file = open(report_filepath, 'r')
for line in report_file.readlines():
    sys.stdout.write('<br>{}'.format(line.strip()))

report_file.close()
if os.path.isfile(report_filepath):
    os.remove(report_filepath)

sys.stdout.write(
    '<script>'
    'if (window.testRunner)'
    '    testRunner.notifyDone();'
    '</script>'
    '</body></html>'
)

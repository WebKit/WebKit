#!/usr/bin/env python3
import os
import sys

sys.stdout.write('Content-Type: text/html\r\n\r\n')

for i in range(64 * 1024):
    sys.stdout.write('<div>text <span style="color:green">{}</span></div>'.format(i))

sys.stdout.write('<script>var i = 0;</script>'.format(i))

for i in range(64 * 1024):
    sys.stdout.write('<div>text <span style="color:blue">{}</span></div>'.format(i))

sys.stdout.write('<script>i = 1;</script>'.format(i))

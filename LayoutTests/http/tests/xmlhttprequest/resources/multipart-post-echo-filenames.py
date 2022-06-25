#!/usr/bin/env python3
import cgi
import sys

sys.stdout.write(
    'Content-Type: text/html\r\n'
    '\r\n'
)

first = True
form = cgi.FieldStorage()
for key in sorted(form.keys()):
    if form[key].filename:
        if not first:
            sys.stdout.write(',')
        sys.stdout.write(form[key].filename)
        first = False

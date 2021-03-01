#!/usr/bin/env python3
import cgi
import os
import sys

sys.stdout.write(
    'Content-Type: text/html\r\n'
    '\r\n'
)
sys.stdout.flush()

if 'multipart/form-data; boundary=' not in os.environ.get('CONTENT_TYPE', ''):
    sys.stdout.write('Invalid Content-Types.')
    sys.exit(0)

form = cgi.FieldStorage()
values =[]
for key in sorted(form.keys()):
    if not form[key].filename and key:
        values.append('{}={}'.format(key, form[key].value))

for key in sorted(form.keys()):
    if not key or not form[key].filename or not form[key].file:
        continue
    else:
        values.append('{}={}:{}:{}'.format(
            key, form[key].filename,
            form[key].type, form[key].file.read().decode(),
        ))

sys.stdout.write('&'.join(values))

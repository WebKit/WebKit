#!/usr/bin/env python3

import base64
import os
import sys

sys.stdout.write('Access-Control-Allow-Origin: *\r\n')
sys.stdout.write('\r\n')
sys.stdout.write('<!DOCTYPE p [')
sys.stdout.write('<!ENTITY passwd SYSTEM "file:///etc/passwd">')
sys.stdout.write('<!ENTITY hosts SYSTEM "file:///etc/hosts">')
sys.stdout.write('<!ENTITY group SYSTEM "file://localhost/etc/group">')
sys.stdout.write(']>')
sys.stdout.write('<p>')
sys.stdout.write('  <p style="border-style: dotted;" id="pw">/etc/passwd:')
sys.stdout.write('&passwd;')
sys.stdout.write('  </p>')
sys.stdout.write(' <p style="border-style: dotted;">/etc/hosts:')
sys.stdout.write('&hosts;')
sys.stdout.write('  </p>')
sys.stdout.write(' <p style="border-style: dotted;">/etc/group:')
sys.stdout.write('&group;')
sys.stdout.write('  </p>')
sys.stdout.write('</p>')
sys.stdout.write('')
sys.exit(0)

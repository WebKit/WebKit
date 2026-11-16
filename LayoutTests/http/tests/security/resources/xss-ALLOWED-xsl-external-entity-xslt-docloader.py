#!/usr/bin/env python3

import base64
import os
import sys

sys.stdout.write('Access-Control-Allow-Origin: *\r\n')
sys.stdout.write('\r\n')
sys.stdout.write('<!DOCTYPE p [')
sys.stdout.write('<!ENTITY entity SYSTEM "xss-ALLOWED-xsl-external-entity-xslt-docloader.xml">')
sys.stdout.write(']>')
sys.stdout.write('<p style="border-style: dotted;" id="entity">&entity;</p>')
sys.stdout.write('')
sys.exit(0)

#!/usr/bin/env python3
import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write('Content-Type: \r\n')
sys.stdout.write('\r\n')

print('<?xml version="1.0" ?>')
print('<root/>')

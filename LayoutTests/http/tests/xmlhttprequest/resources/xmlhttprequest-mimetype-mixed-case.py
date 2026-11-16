#!/usr/bin/env python3
import os
import sys
import time

sys.stdout.write(
    'Content-Type: Application/Xhtml+Xml\r\n'
    '\r\n'
)

file = __file__.split(':/cygwin')[-1]
with open(os.path.join(os.path.dirname(file), 'load-icon.xhtml')) as file:
    sys.stdout.writelines(file.readlines())

#!/usr/bin/env python3

import os
import sys
from ping_file_path import ping_filepath

sys.stdout.write('Content-Type: text/html\r\n\r\n')

if os.path.isfile(ping_filepath):
    os.remove(ping_filepath)

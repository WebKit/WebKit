#!/usr/bin/env python3

import os
import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Status: 302\r\n'
    'Location: network-load-swasdf\r\n\r\n'
)

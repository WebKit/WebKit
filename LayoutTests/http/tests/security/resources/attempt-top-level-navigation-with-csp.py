#!/usr/bin/env python3

import sys
sys.stdout.reconfigure(newline="")  # prevent windows \n -> \n\r conversion

sys.stdout.write(
    'Content-Security-Policy: sandbox allow-scripts allow-top-navigation\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<script>top.location = "http://localhost:8000/security/resources/should-not-have-loaded.html";</script>\n'
)

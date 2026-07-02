#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Type: text/html; charset=UTF-8\r\n'
    "Content-Security-Policy: script-src 'self'; frame-src blob:; default-src 'self'\r\n"
    "Content-Security-Policy: script-src 'self' 'unsafe-inline'; frame-src blob:; default-src 'self'\r\n"
    '\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<body>\n'
    '<iframe id="blob-frame"></iframe>\n'
    '<script src="/security/contentSecurityPolicy/resources/create-blob-iframe.js"></script>\n'
    '</body>\n'
    '</html>\n'
)

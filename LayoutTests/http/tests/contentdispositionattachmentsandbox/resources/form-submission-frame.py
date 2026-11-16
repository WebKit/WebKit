#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Disposition: attachment; filename=test.html\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<style>\n'
    'body {\n'
    '    margin: 0px;\n'
    '}\n'
    '</style>\n'
    '<form>\n'
    '<input type="submit">\n'
    '</form>\n'
)
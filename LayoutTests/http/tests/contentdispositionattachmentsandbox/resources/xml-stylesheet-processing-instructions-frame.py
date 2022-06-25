#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Disposition: attachment; filename=test.xhtml\r\n'
    'Content-Type: application/xhtml+xml\r\n\r\n'
    '<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n'
    '<?xml-stylesheet href=\"data:text/css,body::after { content: \'FAIL\'; }\" ?>\n'
    '<html xmlns=\"http://www.w3.org/1999/xhtml\">\n'
    '<body></body>\n'
    '</html>\n'
)
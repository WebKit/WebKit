#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy: script-src \'unsafe-inline\' \'self\'; style-src \'nonce-test\'\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<body>\n'
    '<p>Test that a stylesheet with a nonce is allowed with CSP served via header</p>\n'
    '<link rel="stylesheet" href="resources/blue.css" nonce="test">'
    '<script>\n'
    '    if (window.testRunner)'
    '       testRunner.dumpAsText();'
    '    document.write(document.styleSheets.length > 0 ? \'PASS\' : \'FAIL\');'
    '</script>\n'
    '\n'
    '</body>\n'
    '</html>\n'
)

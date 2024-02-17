#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy: script-src \'nonce-abc\' \'unsafe-eval\'\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<head>\n'
    '<script nonce="abc" src="../../resources/js-test-pre.js"></script>\n'
    '<svg xmlns="http://www.w3.org/2000/svg">\n'
    '<use id="use" href="#testScript"/>\n'
    '<script nonce="abc">\n'
    '    use.getBoundingClientRect();\n'
    '</script>\n'
    '<script nonce="abc" id="testScript">\n'
    '    document.currentScript.setAttribute("executed", "true");\n'
    '</script>\n'
    '</svg>\n'
    '<script nonce="abc">\n'
    '    description("This tests that nonce content attribute is removed from a SVG script element even if it was a pending resource.");\n'
    '    shouldBeEqualToString(`testScript.getAttribute("nonce")`, "");\n'
    '    shouldBeEqualToString(`testScript.nonce`, "abc");\n'
    '    shouldBeEqualToString(`testScript.getAttribute("executed")`, "true");\n'
    '    successfullyParsed = true;\n'
    '</script>\n'
    '<script nonce="abc" src="../../resources/js-test-post.js"></script>\n'
    '</body>\n'
    '</html>\n'
)

#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: script-src \'self\' \'unsafe-inline\'; report-uri resources/save-report.py\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<body>\n'
    '<script>\n'
    'try {\n'
    '    eval("alert(\'PASS: eval() allowed!\')");\n'
    '} catch (e) {\n'
    '    console.log("FAIL: eval() blocked!");\n'
    '}\n'
    '</script>\n'
    '<script src="resources/go-to-echo-report.js"></script>\n'
    '</body>\n'
    '</html>\n'
)
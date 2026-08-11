#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Type: application/xhtml+xml\r\n'
    'Content-Security-Policy: script-src http://127.0.0.1:8000/resources/redirect.py \'unsafe-inline\'\r\n\r\n'
    '<?xml version="1.0" encoding="UTF-8"?>\n'
    '<?xml-stylesheet type="text/xsl" href="http://127.0.0.1:8000/resources/redirect.py?code=307&amp;url=http%3A%2F%2Flocalhost%3A8000/security/contentSecurityPolicy/resources/alert-fail.xsl"?>\n'
    '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">\n'
    '<html xmlns="http://www.w3.org/1999/xhtml">\n'
    '<head>\n'
    '</head>\n'
    '<body>\n'
    '<script type="text/javascript">\n'
    '//<![CDATA[\n'
    'if (window.testRunner)\n'
    '    testRunner.dumpAsText();\n'
    'alert("PASS");\n'
    '//]]>\n'
    '</script>\n'
    '</body>\n'
    '</html>\n'
)
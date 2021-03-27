#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Type: application/xhtml+xml\r\n'
    'Content-Security-Policy: style-src *; script-src \'unsafe-inline\'\r\n\r\n'
    '<?xml version="1.0" encoding="UTF-8"?>'
    '<?xml-stylesheet type="text/xsl" href="resources/style.xsl"?>'
    '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" \n'
    '        "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">\n'
    '<html xmlns="http://www.w3.org/1999/xhtml">\n'
    '<head>\n'
    '<script>\n'
    '//<![CDATA[\n'
    'if (window.testRunner)\n'
    '    testRunner.dumpAsText();\n'
    '//]]>\n'
    '</script>\n'
    '</head>\n'
    '<body>\n'
    'This test should render as a blank page because the style sheet will fail to load!\n'
    '<div />\n'
    '</body>\n'
    '</html>\n'
)
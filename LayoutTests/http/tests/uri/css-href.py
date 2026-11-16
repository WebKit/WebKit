#!/usr/bin/env python3

import os
import sys

host = os.environ.get('HTTP_HOST', '')

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<html>\n'
    '    <head>\n'
    '        <base href="http://{}/uri/resources/\'">\n'
    '        <style>\n'
    '            @import \'css-href.css\';\n'
    '        </style>\n'
    '    </head>\n'
    '    <body>\n'
    '        <p>Test for <a href="http://bugs.webkit.org/show_bug.cgi?id=11141">bug 11141</a>: \n'
    '        CSS \'@import\' doesn\'t respect HTML Base element.</p>\n'
    '        <p class="c1">This text should be green.</p>\n'
    '        <p>If it is red, the css has been loaded relative to the document.\n'
    '        If it is black, no stylesheet has been rendered, if it is rendered green,\n'
    '        the stylesheet has been rendered correctly from the HREF attribute of the\n'
    '        Base element in the HEAD section of this document.</p>\n'
    '        <p class="c2">This text should also be green.</p>\n'
    '    </body>\n'
    '</html>\n'.format(host)
)
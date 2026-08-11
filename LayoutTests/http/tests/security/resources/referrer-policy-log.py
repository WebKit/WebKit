#!/usr/bin/env python3

import os
import sys

referer = os.environ.get('HTTP_REFERER', '')

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<html>\n'
    '<head>\n'
    '<script>\n'
    'function log(msg) {{\n'
    '    document.getElementById("log").innerHTML += msg + "<br>";\n'
    '}}\n'
    '\n'
    'function runTest() {{\n'
    '    var referrerHeader = "{}";\n'
    '    if (referrerHeader == "")\n'
    '        log("HTTP Referer header is empty");\n'
    '    else\n'
    '        log("HTTP Referer header is " + referrerHeader);\n'
    '\n'
    '    if (document.referrer == "")\n'
    '        log("Referrer is empty");\n'
    '    else\n'
    '        log("Referrer is " + document.referrer);\n'
    '    if (window.opener)\n'
    '        window.opener.postMessage(document.getElementById("log").innerText, "*");\n'
    '    else if (window.testRunner)\n'
    '        testRunner.notifyDone();\n'
    '}}\n'
    '</script>\n'
    '</head>\n'
    '<body onload="runTest()">\n'
    '<div id="log"></div>\n'
    '</body>\n'
    '</html>\n'.format(referer)
)

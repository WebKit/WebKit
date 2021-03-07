#!/usr/bin/env python3

import os
import sys

user_agent = os.environ.get('HTTP_USER_AGENT', '')

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<body>\n'
    '</body>\n'
    '<script>\n'
    'if (window.testRunner) {{\n'
    '    testRunner.dumpAsText();\n'
    '}} else {{\n'
    '    alert("Test can only be run in WebKitTestRunner");\n'
    '}}\n'
    '</script>\n'
    '<script src="resources/user-agent-script.py"></script>\n'
    '<script>\n'
    'window.mainResourceUserAgent = "{}";\n'
    '\n'
    'if (!sessionStorage.savedUserAgent) {{\n'
    '    sessionStorage.savedUserAgent = navigator.userAgent;\n'
    '    testRunner.setCustomUserAgent("WebKitRules");\n'
    '    testRunner.queueReload();\n'
    '}} else {{\n'
    '    errorFound = false;\n'
    '    if (mainResourceUserAgent != subresourceUserAgent) {{\n'
    '        errorFound = true;\n'
    '        document.body.innerHTML = "Error: Main resource and subresource were fetched with different user agent strings.";\n'
    '    }}\n'
    '    if (mainResourceUserAgent == sessionStorage.savedUserAgent) {{\n'
    '        errorFound = true;\n'
    '        document.body.innerHTML += "Error: Main resource was fetched with the same user agent string on the reload as was used on the first load.";\n'
    '    }}\n'
    '    if (subresourceUserAgent == sessionStorage.savedUserAgent) {{\n'
    '        errorFound = true;\n'
    '        document.body.innerHTML += "Error: Subresource was fetched with the same user agent string on the reload as was used on the first load.";\n'
    '    }}\n'
    '    \n'
    '    if (!errorFound)\n'
    '        document.body.innerHTML = "Passed";\n'
    '    \n'
    '    testRunner.notifyDone();\n'
    '}}\n'
    '</script>\n'.format(user_agent)
)
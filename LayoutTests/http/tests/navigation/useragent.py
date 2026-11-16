#!/usr/bin/env python3

import os
import sys

user_agent = os.environ.get('HTTP_USER_AGENT', '')

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<html>\n'
    '<body>\n'
    '\n'
    '<p>Tests for user agent string template</p>\n'
    '\n'
    '<script>\n'
    '    if (window.testRunner) {{\n'
    '        testRunner.dumpAsText();\n'
    '    }}\n'
    '\n'
    '    var userAgent = navigator.userAgent;\n'
    '\n'
    '    // Validate the user agent string using the following template:\n'
    '    var userAgentTemplate = "Mozilla/5.0 (%Platform%%Subplatform%) AppleWebKit/%WebKitVersion% (KHTML, like Gecko)"\n'
    '    var userAgentTemplateRegExp = /^Mozilla\\/5\\.0 \\(([^;]+; )*[^;]+\\) AppleWebKit\\/[0-9\\.]+(\\+)? \\(KHTML, like Gecko\\).*$/;\n'
    '    document.write("UserAgent should match the " + userAgentTemplate + " template: " + !!userAgent.match(userAgentTemplateRegExp) + "<br>");\n'
    '\n'
    '    // Validate navigator.appVersion and navigator.appCodeName\n'
    '    document.write("UserAgent should be the same as the appVersion with appCodeName prefix: " + (userAgent == navigator.appCodeName + "/" + navigator.appVersion) + "<br>");\n'
    '\n'
    '    // Validate HTTP User-Agent header\n'
    '    var userAgentHeader = \'{}\';\n'
    '    document.write("HTTP User-Agent header should be the same as userAgent: " + (userAgentHeader == userAgent) + "<br>");\n'
    '\n'
    '    // Make sure language tag is not present\n'
    '    var languageTagRegExp = new RegExp("[ ;\\(]" + navigator.language + "[ ;\\)]");\n'
    '    document.write("Language tag should not be present in the userAgent: " + !userAgent.match(languageTagRegExp) + "<br>");\n'
    '\n'
    '</script>\n'
    '</body>\n'
    '</html>\n'.format(user_agent)
)
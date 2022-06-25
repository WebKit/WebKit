#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Language: ,es\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html xmlns="http://www.w3.org/1999/xhtml">\n'
    '<head>\n'
    '<script src="../../js-test-resources/js-test-pre.js"></script>\n'
    '</head>\n'
    '<body>\n'
    '<p>Test for <a href="https://bugs.webkit.org/show_bug.cgi?id=97929">bug 97929</a>:\n'
    'Extract HTTP Content-Language header.</p>\n'
    '<div id="console"></div>\n'
    '<div id="x"></div>\n'
    '<div id="y" lang="ar"></div>\n'
    '<script>\n'
    '  debug(\'==> Value extracted from malformed HTTP "Content-Language" header...\');\n'
    '  // A malformed header may be fixed by the browser ("es") or left bad and so remain unset ("auto").\n'
    '  // Chrome does the former; Safari does the latter.\n'
    '  shouldBeTrue(\'window.getComputedStyle(document.getElementById("x")).webkitLocale == "es" || window.getComputedStyle(document.getElementById("x")).webkitLocale == "auto"\')\n'
    '  debug(\'==> Value set by div "lang" tag...\');\n'
    '  shouldBe(\'window.getComputedStyle(document.getElementById("y")).webkitLocale\', \'"ar"\')\n'
    '  debug(\'==> All done...\');\n'
    '</script>\n'
    '<script src="../../js-test-resources/js-test-post.js"></script>\n'
    '</body>\n'
    '</html>\n'
)
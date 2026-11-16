#!/usr/bin/env python3

import sys
import time

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">\n'
    '<html>\n'
    '<head>\n'
    '<script>\n'
    '</script>\n'
    '<script src="../../js-test-resources/js-test-pre.js"></script>\n'
    '</head>\n'
    '<body>\n'
    '<p id="description"></p>\n'
    '<div id="console"></div>\n'
    '<script>\n'
    'description("Verifies that requestStart and responseStart are available before the main document has finished loading.");\n'
    '\n'
    'window.performance = window.performance || {};\n'
    'var navigation = performance.navigation || {};\n'
    'var timing = performance.timing || {};\n'
    '\n'
    'shouldBeNonZero("timing.requestStart");\n'
    'shouldBeNonZero("timing.responseStart");\n'
    'shouldBeNonZero("timing.responseEnd");\n'
    '\n'
    '</script>\n'
    '<script src="../../js-test-resources/js-test-post.js"></script>\n'
)

sys.stdout.write(' ' * 100000)
sys.stdout.flush()
time.sleep(1)

sys.stdout.write(
    '</body>\n'
    '</html>\n'
)

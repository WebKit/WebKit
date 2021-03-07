#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Type: text/html\r\n\r\n'
    '<html>\n'
    '<head>\n'
    '<script>\n'
    'if (window.testRunner)\n'
    '    testRunner.dumpAsText();\n'
    '</script>\n'
    '</head>\n'
    '<body>\n'
    '<h1>This tests verifies that a large program doesn&#39;t crash JavaScript.</h1>\n'
    '<p>This test should generate an out of stack exception, but have no other output.\n'
    '<br>\n'
    '<pre id="console"></pre>\n'
    '<script src="/js-test-resources/js-test-pre.js"></script>\n'
    '<script>\n'
    'function print(m)\n'
    '{\n'
    '    document.getElementById("console").innerHTML += m + "<br>";\n'
    '}\n\n'
    'function foo(o)\n'
    '{\n'
    '    // We should not get to this code, we should throw an out of stack exception calling foo().\n'
    '    testFailed("We should never get here!");\n'
    '}\n\n\n'
    'foo({"x": 1,\n'
    '     "a": [\n'
)

for i in range(0, 1000000):
    if i != 0:
        sys.stdout.write(',\n')
    sys.stdout.write('[0, {}]'.format(i))

sys.stdout.write(
    ']});\n'
    '</script>\n'
    '</body>\n'
    '</html>\n'
)
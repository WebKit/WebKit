#!/usr/bin/env python3

import sys
import time

sys.stdout.write(
    'Content-Type: text/html; charset=utf-8\r\n\r\n'
    '<!-- {} -->\n'
    '<script>\n'
    'if (window.testRunner) {{\n'
    '    testRunner.dumpAsText();\n'
    '    testRunner.dumpResourceResponseMIMETypes();\n'
    '}}\n'
    '</script>\n'
    '<script src="non-existant-1"></script>\n'.format('A' * 2048)
)

sys.stdout.flush()
time.sleep(1)

sys.stdout.write(
    '<script>\n'
    'document.write("<!--");\n'
    '</script>\n'
    '<img src="preload-slow-target.jpg">\n'
    '<script>\n'
    'document.write("-->");\n'
    '</script>\n'
    '<script src="non-existant-2"></script>\n'
)
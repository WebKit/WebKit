#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Content-Security-Policy-Report-Only: script-src \'sha256-AJqUvsXuHfMNXALcBPVqeiKkFk8OLvn3U7ksHP/QQ90=\' \'nonce-dump-as-text\'\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!DOCTYPE html>\n'
    '<html>\n'
    '<head>\n'
    '<meta http-equiv="Content-Security-Policy" content="script-src \'sha256-33badf00d3badf00d3badf00d3badf00d3badf00d33=\' \'nonce-dump-as-text\'">\n'
    '<script nonce="dump-as-text">\n'
    'if (window.testRunner)\n'
    '    testRunner.dumpAsText();\n'
    '</script>\n'
    '</head>\n'
    '<body>\n'
    '<p id="result">PASS did not execute script.</p>\n'
    '<script>\n'
    'document.getElementById("result").textContent = "FAIL did execute script.";\n'
    '</script>\n'
    '</body>\n'
    '</html>\n'
)
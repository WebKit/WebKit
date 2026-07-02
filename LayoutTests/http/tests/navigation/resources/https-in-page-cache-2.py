#!/usr/bin/env python3

import sys

sys.stdout.write(
    'cache-control: no-cacher\r\n'
    'Content-Type: text/html\r\n\r\n'
    '<!-- webkit-test-runner [ enablePageCache=true ] -->\n'
    '<script>\n'
    'function nextTest() {\n'
    '    if (window.sessionStorage.https_in_page_cache_started)\n'
    '        delete window.sessionStorage.https_in_page_cache_started;\n'
    '    location = "https://127.0.0.1:8443/navigation/resources/https-in-page-cache-3.html";\n'
    '}\n'
    '\n'
    'window.onpageshow = function(evt) {\n'
    '    if (!evt.persisted)\n'
    '        return;\n'
    '\n'
    '    alert("The page was restored from the page cache. Good job!. Running part 3 of the test.");\n'
    '    nextTest();\n'
    '}\n'
    '\n'
    'window.onload = function() {\n'
    '    alert("This page is https and has the no-cache cache-control directive. It should go in to the page cache.");\n'
    '    window.sessionStorage.https_in_page_cache_started = true;\n'
    '    setTimeout(\'window.location = "go-back.html"\', 0);\n'
    '\n'
    '}\n'
    '\n'
    '</script>\n'
)
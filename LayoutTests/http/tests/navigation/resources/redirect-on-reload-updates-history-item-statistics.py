#!/usr/bin/env python3

import os
import sys

cookies = {}
if 'HTTP_COOKIE' in os.environ:
    header_cookies = os.environ['HTTP_COOKIE']
    header_cookies = header_cookies.split('; ')

    for cookie in header_cookies:
        cookie = cookie.split('=')
        cookies[cookie[0]] = cookie[1]

sys.stdout.write(
    'cache-control: no-store\r\n'
    'Content-Type: text/html\r\n'
)

location = cookies.get('location', '')
if location:
    sys.stdout.write(
        'status: 303\r\n'
        'Location: {}\r\n\r\n'.format(location)
    )
    sys.exit(0)

sys.stdout.write(
    '\r\n'
    '<script src=\'redirect-updates-history-item.js\'></script>\n'
    '<script>\n'
    'onunload = function() {\n'
    '  // no page cache\n'
    '}\n'
    '\n'
    'onload = function() {\n'
    '    setTimeout(function() {\n'
    '        // The first time through here (sessionStorage.done is false), this\n'
    '        // code inserts a new history item using pushState, and then it\n'
    '        // triggers a reload of the history item.  However, we set the\n'
    '        // "location" cookie so that when we reload this page, we actually\n'
    '        // redirect to the value of the "location" cookie.\n'
    '        //\n'
    '        // This loads the "goback" page, which bounces us back here after\n'
    '        // setting sessionStorage.done to true.  The point of this test is to\n'
    '        // ensure that going back actually performs a real navigation as\n'
    '        // opposed to performing a "same document navigation" as would normally\n'
    '        // be done when navigating back after a pushState.\n'
    '\n'
    '        if (sessionStorage.done) {\n'
    '            location.replace("redirect-updates-history-item-done-statistics.html"); \n'
    '        } else {\n'
    '            history.pushState(null, null, "");\n'
    '\n'
    '            setLocationCookie("redirect-on-reload-updates-history-item-goback.html");\n'
    '            location.reload();\n'
    '        }\n'
    '    }, 0);\n'
    '}\n'
    '</script>\n'
    '\n'
    '<p>redirect-on-reload-updates-history-item.py: You should not see this text!</p>\n'
)
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
        'Location: {}\r\n'
        'status: 303\r\n\r\n'.format(location)
    )
    sys.exit(0)


sys.stdout.write(
    '\n'
    '<script src=\'redirect-updates-history-item.js\'></script>\n'
    '<script>\n'
    'onunload = function() {\n'
    '    // no page cache\n'
    '}\n'
    '\n'
    'onload = function() {\n'
    '    setTimeout(function() {\n'
    '        // This code inserts a new history item using pushState, and then it\n'
    '        // replaces that history item with a navigation to a page that just\n'
    '        // navigates us back to this page.  However, we set the "location"\n'
    '        // cookie so that when we navigate back to this page, we actually\n'
    '        // redirect to the value of the "location" cookie.\n'
    '\n'
    '        setLocationCookie("redirect-updates-history-item-done.html");\n'
    '\n'
    '        history.pushState(null, null, "");\n'
    '        location.replace("redirect-on-back-updates-history-item-goback.html");\n'
    '    }, 0);\n'
    '}\n'
    '</script>\n'
    '\n'
    '<p>redirect-on-back-updates-history-item.py: You should not see this text!</p>\n'
)
#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs

query = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True)
csp = query.get('csp', [None])[0]
typ = query.get('type', [''])[0]

sys.stdout.write(
    'Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n'
    'Cache-Control: no-cache, must-revalidate\r\n'
    'Pragma: no-cache\r\n'
    'Content-Type: text/javascript\r\n'
)

if csp is not None:
    sys.stdout.write('Content-Security-Policy: {}\r\n'.format(csp))
elif typ == 'multiple-headers':
    sys.stdout.write(
        'Content-Security-Policy: connect-src \'none\'\r\n'
        'Content-Security-Policy: script-src \'self\'\r\n'
    )

sys.stdout.write('\r\n')

if typ == 'eval':
    sys.stdout.write(
        'var id = 0;\n'
        'try {\n'
        '  id = eval("1 + 2 + 3");\n'
        '}\n'
        '  catch (e) {\n'
        '}\n'
        '\n'
        'postMessage(id === 0 ? "eval blocked" : "eval allowed");\n'
    )

elif typ == 'function-function':
    sys.stdout.write(
        'var fn = function() {\n'
        '    postMessage(\'Function() function blocked\');\n'
        '}\n'
        'try {\n'
        '    fn = new Function("", "postMessage(\'Function() function allowed\');");\n'
        '}\n'
        'catch(e) {\n'
        '}\n'
        'fn();\n'
    )

elif typ == 'importscripts':
    sys.stdout.write(
        'try {\n'
        '    importScripts("http://localhost:8000/security/contentSecurityPolicy/resources/post-message.js");\n'
        '    postMessage("importScripts allowed");\n'
        '} catch(e) {\n'
        '    postMessage("importScripts blocked: " + e);\n'
        '}\n'
    )

elif typ == 'make-xhr':
    sys.stdout.write(
        'var xhr = new XMLHttpRequest;\n'
        'xhr.addEventListener("load", function () {\n'
        '    postMessage("xhr allowed");\n'
        '});\n'
        'xhr.addEventListener("error", function () {\n'
        '    postMessage("xhr blocked");\n'
        '});\n'
        'xhr.open("GET", "http://127.0.0.1:8000/xmlhttprequest/resources/get.txt", true);\n'
        'xhr.send();\n'
    )

elif typ == 'set-timeout':
    sys.stdout.write(
        'var id = 0;\n'
        'try {\n'
        '    id = setTimeout("postMessage(\'handler invoked\')", 100);\n'
        '} catch(e) {\n'
        '}\n'
        'postMessage(id === 0 ? "setTimeout blocked" : "setTimeout allowed");\n'
    )

elif typ == 'post-message-pass':
    sys.stdout.write('postMessage("PASS");')

elif typ == 'report-referrer':
    sys.stdout.write(
        'var xhr = new XMLHttpRequest;\n'
        'xhr.open("GET", "http://127.0.0.1:8000/security/resources/echo-referrer-header.py", true);\n'
        'xhr.onload = function () {\n'
        '    postMessage(this.responseText);\n'
        '};\n'
        'xhr.send();\n'
    )

elif typ == 'shared-report-referrer':
    sys.stdout.write(
        'onconnect = function (e) {\n'
        '    var port = e.ports[0];\n'
        '    var xhr = new XMLHttpRequest;\n'
        '    xhr.open(\n'
        '        "GET",\n'
        '        "http://127.0.0.1:8000/security/resources/echo-referrer-header.py",\n'
        '        true);\n'
        '    xhr.onload = function () {\n'
        '        port.postMessage(this.responseText);\n'
        '    };\n'
        '    xhr.send();\n'
        '};\n'
    )

elif typ == 'multiple-headers':
    sys.stdout.write(
        'var xhr = new XMLHttpRequest;\n'
        'xhr.addEventListener("load", function () {\n'
        '    postMessage("xhr allowed");\n'
        '});\n'
        'xhr.addEventListener("error", function () {\n'
        '    postMessage("xhr blocked");\n'
        '});\n'
        'xhr.open("GET", "http://127.0.0.1:8000/xmlhttprequest/resources/get.txt", true);\n'
        'xhr.send();\n'
        '\n'
        'var id = 0;\n'
        'try {\n'
        '  id = eval("1 + 2 + 3");\n'
        '}\n'
        'catch (e) {\n'
        '}\n'
        '\n'
        'postMessage(id === 0 ? "eval blocked" : "eval allowed");\n'
    )

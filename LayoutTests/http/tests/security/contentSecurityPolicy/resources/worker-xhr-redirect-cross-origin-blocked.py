#!/usr/bin/env python3

import sys
from utils import determine_content_security_policy_header

determine_content_security_policy_header()
sys.stdout.write(
    'Content-Type: text/javascript\r\n\r\n'
    'var expectedExceptionCode = 19; // DOMException.NETWORK_ERR\n'
    'var isAsynchronous = false;\n'
    'var xhr = new XMLHttpRequest;\n'
    'try {\n'
    '    xhr.open("GET", "http://127.0.0.1:8000/security/contentSecurityPolicy/resources/redir.py?url=http://localhost:8000/xmlhttprequest/resources/access-control-basic-allow.cgi", isAsynchronous);\n'
    '    xhr.send();\n'
    '    self.postMessage("FAIL should throw " + expectedExceptionCode + ". But did not throw an exception.");\n'
    '} catch (exception) {\n'
    '    if (exception.code === expectedExceptionCode)\n'
    '        self.postMessage("PASS threw exception " + exception + ".");\n'
    '    else\n'
    '        self.postMessage("FAIL should throw " + expectedExceptionCode + ". Threw exception " + exception + ".");\n'
    '}\n'
)

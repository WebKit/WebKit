#!/usr/bin/env python3

import sys
from utils import determine_content_security_policy_header

sys.stdout.write(
    'Content-Type: text/javascript\r\n\r\n'
    'var isAsynchronous = false;\n'
    'var xhr = new XMLHttpRequest;\n'
    'try {\n'
    '    xhr.open("GET", "http://127.0.0.1:8000/security/contentSecurityPolicy/resources/redir.py?url=http://localhost:8000/xmlhttprequest/resources/access-control-basic-allow.cgi", isAsynchronous);\n'
    '    xhr.send();\n'
    '    self.postMessage(xhr.response);\n'
    '} catch (exception) {\n'
    '    self.postMessage("FAIL should not have thrown an exception. Threw exception " + exception + ".");\n'
    '}\n'
)

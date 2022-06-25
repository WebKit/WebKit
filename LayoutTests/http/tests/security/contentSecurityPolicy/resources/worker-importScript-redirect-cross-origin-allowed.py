#!/usr/bin/env python3

import sys
from utils import determine_content_security_policy_header

determine_content_security_policy_header()
sys.stdout.write(
    'Content-Type: text/javascript\r\n\r\n'
    'self.result = false;\n'
    'var exception;\n'
    'try {\n'
    '    importScripts("http://127.0.0.1:8000/security/contentSecurityPolicy/resources/redir.py?url=http://localhost:8000/security/contentSecurityPolicy/resources/script-set-value.js");\n'
    '} catch (e) {\n'
    '    exception = e;\n'
    '}\n'
    'if (exception)\n'
    '    self.postMessage("FAIL should not have thrown an exception. Threw exception " + exception + ".");\n'
    'else {\n'
    '    if (self.result)\n'
    '        self.postMessage("PASS did import script from different origin.");\n'
    '    else\n'
    '        self.postMessage("FAIL did not import script from different origin.");\n'
    '}\n'
)

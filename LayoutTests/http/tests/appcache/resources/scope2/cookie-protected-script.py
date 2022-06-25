#!/usr/bin/env python3

import os
import sys

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file)))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies

cookies = get_cookies()
scope = cookies.get('scope', None)

sys.stdout.write('Content-type:application/javascript\r\n\r\n')

if scope is not None:
    sys.stdout.write('var cookieVal = \'{}\';'.format(scope))
else:
    sys.stdout.write('var cookieVal = \'<null>\';')

print('''
if (cookieVal === "script")
    document.getElementById("result1").innerHTML = "PASSED: Script Cookie is set to 'script'";
else
    document.getElementById("result1").innerHTML = "FAILED: Script Cookie should be 'script', is set to '" + cookieVal + "'";''')

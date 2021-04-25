#!/usr/bin/env python3

import os
import sys

file = __file__.split(':/cygwin')[-1]
http_root = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(file))))
sys.path.insert(0, http_root)

from resources.portabilityLayer import get_cookies

cookies = get_cookies()
foo = cookies.get('foo', None)

sys.stdout.write('Content-type:application/javascript\r\n\r\n')

if foo is not None:
    sys.stdout.write('var cookieVal = \'{}\';'.format(foo))
else:
    sys.stodut.write('var cookieVal = \'<null>\';')

print('''
if (cookieVal == "bar")
    document.getElementById("result").innerHTML = "PASSED: Cookie is set to 'bar'";
else
    document.getElementById("result").innerHTML = "FAILED: Cookie should be 'bar', is set to '" + cookieVal + "'";

if (window.testRunner)
    testRunner.notifyDone();''')
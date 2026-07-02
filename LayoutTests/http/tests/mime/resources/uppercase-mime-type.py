#!/usr/bin/env python3

import sys

sys.stdout.write('Content-Type: TEXT/HTML\r\n\r\n')

print('<html>')
print('<script>')
print('if (window.testRunner)')
print('   testRunner.notifyDone();')
print('')
print('</script>')
print('<body>')
print('If this text is shown, that means the new document was successfully loaded.')
print('</body>')
print('</html>')
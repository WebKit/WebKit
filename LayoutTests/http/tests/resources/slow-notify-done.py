#!/usr/bin/env python3

import sys
import time

time.sleep(0.1)

sys.stdout.write('Content-Type: text/html\r\n\r\n')
print('''DONE!
<script>
if (window.testRunner)
    testRunner.notifyDone();
</script>''')

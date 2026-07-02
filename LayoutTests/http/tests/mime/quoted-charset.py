#!/usr/bin/env python3

import sys

sys.stdout.write('Content-Type: text/html; charset="utf-8"\r\n\r\n')

print('''<p>Should say SUCCESS: {}</p>
<p>The latter has some Cyrillic characters that look like Latin ones. This test verifies that decoding
is correctly performed.</p>

<script>
if (window.testRunner)
    testRunner.dumpAsText();
</script>'''.format('SUóóåSS'.encode('ISO-8859-1').decode('KOI8-R')))
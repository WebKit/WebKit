#!/usr/bin/env python3

import sys

sys.stdout.write(
    'X-FRAME-OPTIONS: deny\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<html manifest="resources/x-frame-options-prevents-framing.manifest">
<script>

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.dumpChildFramesAsText();
    testRunner.waitUntilDone();
}

function cached()
{
     window.location.href = "/appcache/resources/x-frame-options-prevents-framing-test.html";
}
applicationCache.addEventListener('cached', cached, false);

</script>
This document should not be frameable.<br>
If you see this text in an iframe, then there is a bug.<br>
</html>''')
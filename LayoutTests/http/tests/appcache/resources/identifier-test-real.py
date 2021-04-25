#!/usr/bin/env python3

import sys

sys.stdout.write(
    'X-Frame-Options: SAMEORIGIN\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<html manifest="identifier-test.manifest">
<script>

var sentMessage = false;
function cached()
{
    if (sentMessage)
        return;

    sentMessage = true;
    window.opener.postMessage("Nice", "*");
}

applicationCache.addEventListener('cached', cached, false);
applicationCache.addEventListener('noupdate', cached, false);

</script>
</html>''')
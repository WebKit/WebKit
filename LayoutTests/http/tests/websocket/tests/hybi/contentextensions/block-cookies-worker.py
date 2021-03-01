#!/usr/bin/env python3


import os
import sys
from datetime import datetime, timedelta
from urllib.parse import parse_qs

clear = parse_qs(os.environ.get('QUERY_STRING', ''), keep_blank_values=True).get('clear', [None])[0]
clear_exp_time = datetime.utcnow() - timedelta(seconds=1)
exp_time = datetime.utcnow() + timedelta(hours=1)

if clear:
    sys.stdout.write(
        'Content-Type: text/plain\r\n'
        'Set-Cookie: "WK-websocket-test"=0; expires={} GMT; Max-Age=0\r\n'
        'Set-Cookie: "WK-websocket-test-httponly"=0; expires={} GMT; Max-Age=0; HttpOnly\r\n\r\n'
        'Cookies are cleared.'.format(clear_exp_time.strftime('%a, %d-%b-%Y %H:%M:%S'))
    )

    sys.exit(0)

sys.stdout.write(
    'Content-Type: text/html\r\n'
    'Set-Cookie: "WK-websocket-test"=1\r\n'
    'Set-Cookie: "WK-websocket-test-httponly"=1; expires={} GMT; Max-Age=3600; HttpOnly\r\n'
    'Set-Cookie: "WK-websocket-test"=1\r\n'
    'Set-Cookie: "WK-websocket-test-httponly"=1; HttpOnly\r\n\r\n'.format(exp_time.strftime('%a, %d-%b-%Y %H:%M:%S'))
)

print('''<html>
<head>
<script src="../../../../../js-test-resources/js-test-pre.js"></script>
</head>
<body>
<p>Tests that WebSocket sends no cookies when they are blocked.</p>
<p>On success, you will see a series of "PASS" messages, followed by "TEST COMPLETE".</p>
<div id="console"></div>
<script>
window.jsTestIsAsync = true;

function startsWith(str, prefix) {
    return str.indexOf(prefix) == 0;
}

var worker = new Worker("resources/block-cookies-worker.js");
worker.onmessage = function (event)
{
    var message = event.data;
    debug(message);
    if (startsWith(message, "PASS") || startsWith(message, "FAIL"))
        finishJSTest();
};

</script>
<script src="../../../../../js-test-resources/js-test-post.js"></script>
</body>
</html>''')
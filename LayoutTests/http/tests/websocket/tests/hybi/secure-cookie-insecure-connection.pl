#!/usr/bin/perl -wT
use strict;
binmode STDOUT;

if ($ENV{"QUERY_STRING"} eq "clear=1") {
    print "Content-Type: text/plain\r\n";
    print "Set-Cookie: WK-websocket-test=0; Max-Age=-1\r\n";
    print "Set-Cookie: WK-websocket-test-secure=0; Secure; Max-Age=-1\r\n";
    print "\r\n";
    print "Cookies are cleared.";
    exit;
}

print "Content-Type: text/html\r\n";
print "Set-Cookie: WK-websocket-test=1\r\n";
print "Set-Cookie: WK-websocket-test-secure=1; Secure\r\n";
print "\r\n";
print <<HTML
<html>
<head>
<script src="../../../../js-test-resources/js-test-pre.js"></script>
</head>
<body>
<p>Test WebSocket does not send Secure cookies over an insecure connection.</p>
<p>On success, you will see a series of "PASS" messages, followed by "TEST COMPLETE".</p>
<div id="console"></div>
<script>
window.jsTestIsAsync = true;

var cookie;

// Normalize a cookie string
function normalizeCookie(cookie)
{
    // Split the cookie string, sort it and then put it back together.
    return cookie.split('; ').sort().join('; ');
}

function clearCookies()
{
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "secure-cookie-insecure-connection.pl?clear=1", false);
    xhr.send(null);
}

var ws = new WebSocket("ws://127.0.0.1:8880/websocket/tests/hybi/echo-cookie");
ws.onopen = function() {
    debug("WebSocket open");
};
ws.onmessage = function(evt) {
    cookie = evt.data;
    ws.close();
};
ws.onclose = function() {
    debug("WebSocket closed");
    cookie = normalizeCookie(cookie);
    shouldBe("cookie", '"WK-websocket-test=1"');
    clearCookies();
    finishJSTest();
};

</script>
<script src="../../../../js-test-resources/js-test-post.js"></script>
</body>
</html>
HTML

#!/usr/bin/perl -wT
use strict;

if ($ENV{"QUERY_STRING"} eq "clear=1") {
    print "Content-Type: text/plain\r\n";
    print "Set-Cookie: WK-websocket-test=0; Max-Age=0\r\n";
    print "Set-Cookie: WK-websocket-test-httponly=0; HttpOnly; Max-Age=0\r\n";
    print "\r\n";
    print "Cookies are cleared.";
    exit;
}

print "Content-Type: text/html\r\n";
print "Set-Cookie: WK-websocket-test=1\r\n";
print "Set-Cookie: WK-websocket-test-httponly=1; HttpOnly\r\n";
print "\r\n";
print <<HTML
<html>
<head>
<script src="../../../../js-test-resources/js-test-pre.js"></script>
</head>
<body>
<p>Test WebSocket sends HttpOnly cookies.</p>
<p>On success, you will see a series of "PASS" messages, followed by "TEST COMPLETE".</p>
<div id="console"></div>
<script>
window.jsTestIsAsync = true;
if (window.layoutTestController)
    layoutTestController.overridePreference("WebKitHixie76WebSocketProtocolEnabled", 0);

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
    xhr.open("GET", "httponly-cookie.pl?clear=1", false);
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
    shouldBe("cookie", '"WK-websocket-test-httponly=1; WK-websocket-test=1"');
    clearCookies();
    finishJSTest();
};

</script>
<script src="../../../../js-test-resources/js-test-post.js"></script>
</body>
</html>
HTML

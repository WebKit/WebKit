#!/usr/bin/perl -wT
use strict;
binmode STDOUT;

if ($ENV{"QUERY_STRING"} eq "clear=1") {
    print "Content-Type: text/plain\r\n";
    print "Set-Cookie: WK-websocket-test=0; Max-Age=-1\r\n";
    print "Set-Cookie: WK-websocket-test-secure=0; Secure; HttpOnly; Max-Age=-1\r\n";
    print "\r\n";
    print "Cookies are cleared.";
    exit;
}

print "Content-Type: text/html\r\n";
print "Set-Cookie: WK-websocket-test=1\r\n";
print "Set-Cookie: WK-websocket-test-secure=1; Secure; HttpOnly\r\n";
print "\r\n";
print <<HTML
<html>
<head>
<script src="../../../../js-test-resources/js-test-pre.js"></script>
</head>
<body>
<p>Test WebSocket sends Secure cookies over secure connections.</p>
<p>On success, you will see a series of "PASS" messages, followed by "TEST COMPLETE".</p>
<p>Note: mod_pywebsocket does not send secure cookies ('see FIXMEs in mod_pywebsocket code'), so this test is expected to FAIL to return WK-websocket-test-secure=1 until that bug is fixed.</p>
<p>See <a href="https://github.com/google/pywebsocket/issues/150">pywebsocket Bug 150</a> for details.</p>
<div id="console"></div>
<script>
window.jsTestIsAsync = true;

if (window.testRunner)
    testRunner.setAllowsAnySSLCertificate(true);

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
    xhr.open("GET", "secure-cookie-secure-connection.pl?clear=1", false);
    xhr.send(null);
}

var ws = new WebSocket("wss://127.0.0.1:9323/websocket/tests/hybi/echo-cookie");
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
    shouldBe("cookie", '"WK-websocket-test-secure=1; WK-websocket-test=1"');
    clearCookies();
    finishJSTest();
};

</script>
<script src="../../../../js-test-resources/js-test-post.js"></script>
</body>
</html>
HTML

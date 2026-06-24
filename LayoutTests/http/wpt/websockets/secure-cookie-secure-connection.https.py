def main(request, response):
    query = request.url_parts.query or ""

    if query == "clear=1":
        return (
            [
                (b"Content-Type", b"text/plain"),
                (b"Set-Cookie", b"WK-websocket-test=0; Path=/; Max-Age=-1"),
                (b"Set-Cookie", b"WK-websocket-test-secure=0; Path=/; Secure; HttpOnly; Max-Age=-1"),
            ],
            b"Cookies are cleared.",
        )

    # Phase 1: bounce away from localhost to https://web-platform.test. Loading
    # the response over https (with a non-loopback hostname) is what lets the
    # Secure cookie be set in a way that does not depend on CFNetwork's loopback
    # "potentially trustworthy" treatment.
    if query != "phase=test":
        return (
            [(b"Content-Type", b"text/html")],
            b"""<html><body><script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
    testRunner.setAllowsAnySSLCertificate(true);
}
location.href = "https://web-platform.test:9443/WebKit/websockets/secure-cookie-secure-connection.https.py?phase=test";
</script></body></html>""",
        )

    # Phase 2: served over https://web-platform.test, set both cookies and
    # verify both are sent over wss.
    return (
        [
            (b"Content-Type", b"text/html"),
            (b"Set-Cookie", b"WK-websocket-test=1; Path=/"),
            (b"Set-Cookie", b"WK-websocket-test-secure=1; Path=/; Secure; HttpOnly"),
        ],
        b"""<html>
<head>
<script src="/webkit-test-resources/js-test-pre.js"></script>
</head>
<body>
<p>Test WebSocket sends Secure cookies over secure connections.</p>
<p>On success, you will see a series of "PASS" messages, followed by "TEST COMPLETE".</p>
<div id="console"></div>
<script>
window.jsTestIsAsync = true;

var cookie;

function normalizeCookie(c) { return c.split('; ').sort().join('; '); }

function clearCookies() {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "https://web-platform.test:9443/WebKit/websockets/secure-cookie-secure-connection.https.py?clear=1", false);
    xhr.send(null);
}

var ws = new WebSocket("wss://web-platform.test:49002/echo-cookie");
ws.onopen = function() { debug("WebSocket open"); };
ws.onmessage = function(evt) { cookie = evt.data; ws.close(); };
ws.onclose = function() {
    debug("WebSocket closed");
    cookie = normalizeCookie(cookie);
    shouldBe("cookie", '"WK-websocket-test-secure=1; WK-websocket-test=1"');
    clearCookies();
    finishJSTest();
};
</script>
<script src="/webkit-test-resources/js-test-post.js"></script>
</body>
</html>""",
    )

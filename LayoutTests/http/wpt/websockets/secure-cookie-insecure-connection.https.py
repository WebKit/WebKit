def main(request, response):
    query = request.url_parts.query or ""

    if query == "clear=1":
        return (
            [
                (b"Content-Type", b"text/plain"),
                (b"Access-Control-Allow-Origin", b"http://web-platform.test:8800"),
                (b"Access-Control-Allow-Credentials", b"true"),
                (b"Set-Cookie", b"WK-websocket-test=0; Path=/; Max-Age=-1"),
                (b"Set-Cookie", b"WK-websocket-test-secure=0; Path=/; Secure; Max-Age=-1"),
            ],
            b"Cookies are cleared.",
        )

    # Phase 1: bounce away from localhost to https://web-platform.test. CFNetwork
    # treats loopback hosts (127.0.0.0/8, ::1, localhost, *.localhost) as
    # "potentially trustworthy", which means a Secure cookie can be set over
    # plain http and is also sent over plain ws to a loopback host. Using a
    # non-loopback hostname ensures the connection is treated as truly insecure
    # for cookie purposes.
    if query != "phase=cookies" and query != "phase=verify":
        return (
            [(b"Content-Type", b"text/html")],
            b"""<html><body><script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
    testRunner.setAllowsAnySSLCertificate(true);
}
location.href = "https://web-platform.test:9443/WebKit/websockets/secure-cookie-insecure-connection.https.py?phase=cookies";
</script></body></html>""",
        )

    # Phase 2: served over https://web-platform.test, set both cookies (Secure
    # attribute is honored because the response is over https) then bounce to
    # http://web-platform.test for the actual ws test.
    if query == "phase=cookies":
        return (
            [
                (b"Content-Type", b"text/html"),
                (b"Set-Cookie", b"WK-websocket-test=1; Path=/"),
                (b"Set-Cookie", b"WK-websocket-test-secure=1; Path=/; Secure"),
            ],
            b"""<html><body><script>
location.href = "http://web-platform.test:8800/WebKit/websockets/secure-cookie-insecure-connection.https.py?phase=verify";
</script></body></html>""",
        )

    # Phase 3: served over http://web-platform.test, open ws (truly insecure --
    # non loopback hostname, insecure scheme) and verify only the non-Secure
    # cookie is sent.
    return (
        [(b"Content-Type", b"text/html")],
        b"""<html>
<head>
<script src="/webkit-test-resources/js-test-pre.js"></script>
</head>
<body>
<p>Test WebSocket does not send Secure cookies over an insecure connection.</p>
<p>On success, you will see a series of "PASS" messages, followed by "TEST COMPLETE".</p>
<div id="console"></div>
<script>
window.jsTestIsAsync = true;

var cookie;

function normalizeCookie(c) { return c.split('; ').sort().join('; '); }

function clearCookies() {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "https://web-platform.test:9443/WebKit/websockets/secure-cookie-insecure-connection.https.py?clear=1", false);
    xhr.send(null);
}

var ws = new WebSocket("ws://web-platform.test:49001/echo-cookie");
ws.onopen = function() { debug("WebSocket open"); };
ws.onmessage = function(evt) { cookie = evt.data; ws.close(); };
ws.onclose = function() {
    debug("WebSocket closed");
    cookie = normalizeCookie(cookie);
    shouldBe("cookie", '"WK-websocket-test=1"');
    clearCookies();
    finishJSTest();
};
</script>
<script src="/webkit-test-resources/js-test-post.js"></script>
</body>
</html>""",
    )

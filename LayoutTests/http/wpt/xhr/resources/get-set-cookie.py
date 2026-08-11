def main(request, response):
    headers = [(b"Content-Type", b"text/plain")]

    if request.url_parts.query:
        # Assume any query string means "?clear=1": expire both cookies.
        age = b"; expires=Thu, 19 Mar 1982 11:22:11 GMT"
        # Permit the cross-origin clean-up XHR from http://web-platform.test
        # used by the multi-phase cookies.https.html test that bounces between
        # http and https.
        headers.append((b"Access-Control-Allow-Origin", b"http://web-platform.test:8800"))
        headers.append((b"Access-Control-Allow-Credentials", b"true"))
    else:
        age = b""

    headers.append((b"Set-Cookie", b"WK-test=1" + age))
    headers.append((b"Set-Cookie", b"WK-test-secure=1; secure" + age))

    body = request.headers.get(b"Cookie", b"")
    return headers, body

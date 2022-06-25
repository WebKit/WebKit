import sys
import urllib.parse

def main(request, response):
    """
    Returns cookie name and path from query params in a Set-Cookie header.
    e.g.
    > GET /WebKit/service-workers/resources/set-cookie.py?name=match-slash&path=%2F HTTP/1.1
    > Host: localhost:8000
    > User-Agent: curl/7.43.0
    > Accept: */*
    >
    < HTTP/1.1 200 OK
    < Content-Type: application/json
    < Set-Cookie: match-slash=1; Path=/; Expires=Wed, 09 Jun 2021 10:18:14 GMT
    < Server: BaseHTTP/0.3 Python/2.7.12
    < Date: Tue, 04 Oct 2016 18:16:06 GMT
    < Content-Length: 80
    """
    params = urllib.parse.parse_qs(request.url_parts.query)
    headers = [
        (b"Content-Type", b"application/json"),
        (b"Set-Cookie", b"{name[0]}=1; Path={path[0]}; Expires=Wed, 09 Jun 2021 10:18:14 GMT".format(**params))
    ]
    body = "{}"
    return headers, body

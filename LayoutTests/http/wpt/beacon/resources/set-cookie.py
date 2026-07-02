
import sys
import urlparse

def main(request, response):
    params = urlparse.parse_qs(request.url_parts.query)
    headers = [
        ("Content-Type", "application/json"),
        ("Cache-Control", "no-store"),
        ("Access-Control-Allow-Origin", "*"),
        ("Set-Cookie", "{name[0]}=1; Path={path[0]}; Expires=Wed, 09 Jun 2021 10:18:14 GMT".format(**params))
    ]
    body = "{}"
    return headers, body

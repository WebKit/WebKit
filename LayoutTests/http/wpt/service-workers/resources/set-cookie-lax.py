import sys
from urllib.parse import urlparse, parse_qs


def main(request, response):
    params = parse_qs(request.url_parts.query)
    headers = [
        ("Content-Type", "application/json"),
        ("Set-Cookie", "{name[0]}=1; path=/; secure; HttpOnly; SameSite=Lax".format(**params))
    ]
    body = "{}"
    return headers, body

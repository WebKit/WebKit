import os
import hashlib
import json


def main(request, response):
    token = request.GET[b'token']

    if request.method == "POST":
        request.server.stash.take(token, "body-value")
        request.server.stash.put(token, request.GET[b'value'], "body-value")
        response.headers.set(b"Content-Type", b"text/ascii")
        return b"updated to " + request.GET[b'value']

    if request.GET.first(b"count", False):
        count = request.server.stash.take(token, "count-value")
        if not count:
            count = -1
        response.headers.set(b"Content-Type", b"text/ascii")
        return str(count)

    count = request.server.stash.take(token, "count-value")
    if not count:
        count = 0
    count = count + 1
    request.server.stash.put(token, count, "count-value")

    value = request.server.stash.take(token, "body-value")
    if not value:
        value = bytes("nothing for " + token.decode("utf-8"), "utf-8")

    if b"cache" in request.GET:
        response.headers.set(b"Cache-Control", b"max-age=31536000")
    else:
        response.headers.set(b"Cache-Control", b"no-cache")

    if b"download" in request.GET:
        response.headers.set(b"Content-Type", b"text/vcard")
        return value.decode()

    if b"customHeader" in request.GET:
        value = request.headers.get(b"x-custom-header", b"no custom header")

    response.headers.set(b"Content-Type", b"text/html")
    return "<html><body><script>window.value = '%s';</script></body></html>" % value.decode()

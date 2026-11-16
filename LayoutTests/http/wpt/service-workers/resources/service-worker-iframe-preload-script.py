import os
import hashlib
import json


def main(request, response):
    token = request.GET[b'token']
    testId = hashlib.md5(token).hexdigest()

    if request.method == "POST":
        request.server.stash.take(testId)
        request.server.stash.put(testId, request.GET[b'value'])
        response.headers.set(b"Content-Type", b"text/ascii")
        return b"updated to " + request.GET[b'value']

    value = request.server.stash.take(testId)
    if not value:
        response.headers.set(b"Cache-Control", b"no-cache")
        response.headers.set(b"Content-Type", b"text/html")
        return "nothing"

    response.headers.set(b"Cache-Control", b"no-cache")
    response.headers.set(b"Content-Type", b"text/ascii")
    return value.decode()

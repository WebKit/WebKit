import json


def main(request, response):
    token = request.GET.first(b"token")
    data = request.server.stash.take(token)
    if not data:
        data = ""

    if b"get_referrer" in request.GET:
        return 200, [(b"Content-Type", b"text/plain2")], data

    request.server.stash.put(token, request.headers.get(b"Referer", None).decode("utf-8"))

    headers = [(b"Content-Type", b"text/javascript"),
               (b"Cache-Control", b"no-cache"),
               (b"Pragma", b"no-cache")]

    return 200, headers, u"onfetch = e => { }"

def main(request, response):
    token = request.GET.first(b"token", None)
    if request.method == u'OPTIONS':
        headers = request.headers.get("Access-Control-Request-Headers", "none")
        if "cache-control" in str(headers):
            request.server.stash.put(token, "preflight ko, got cache-control")
            return 404, [(b"Cache-Control", b"no-store")], u"cache-control in preflight"
        if "content-type" not in str(headers):
            request.server.stash.put(token, "preflight ko got content-type")
            return 404, [(b"Cache-Control", b"no-store")], u"content-type not in preflight"
        request.server.stash.put(token, "preflight ok")
        return 200, [(b"Cache-Control", b"no-store"), (b"Content-Type", b"text/ascii"), (b"Access-Control-Allow-Origin", b"http://localhost:8800"), (b"Access-Control-Allow-Credentials", b"true"), (b"Access-Control-Allow-Headers", b"content-type"), (b"Access-Control-Allow-Methods", b"*")], u"ok"
    if request.method == u'GET':
        return 200, [(b"Content-Type", b"text/ascii")], request.server.stash.take(token)
    return 200, [(b"Content-Type", b"text/ascii"), (b"Access-Control-Allow-Origin", b"http://localhost:8800"), (b"Access-Control-Allow-Credentials", b"true")], u"post ok"

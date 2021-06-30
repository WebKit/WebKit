from wptserve.utils import isomorphic_decode

def main(request, response):
    headers = [(b"Content-Type", b"text/plain"),
               (b"Cache-Control", b"no-cache"),
               (b"Pragma", b"no-cache")]

    if b"store" in request.GET:
        request.server.stash.put(request.GET[b'token'], isomorphic_decode(request.headers.get(b"DNT", b"-1")))
        return 200, headers, ""

    return 200, headers, str(request.server.stash.take(request.GET[b'token']))

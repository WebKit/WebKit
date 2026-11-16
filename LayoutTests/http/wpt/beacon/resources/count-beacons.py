def main(request, response):
    token = request.GET.first(b'token')
    record = request.GET.first(b'record', None) is not None

    with request.server.stash.lock:
        count = request.server.stash.take(token) or 0
        if record:
            count += 1
        request.server.stash.put(token, count)

    return [(b'Cache-Control', b'no-store'), (b'Access-Control-Allow-Origin', b'*'), (b'Content-Type', b'text/plain')], str(count)

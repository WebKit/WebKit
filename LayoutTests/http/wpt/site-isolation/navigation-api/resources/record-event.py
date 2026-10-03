def main(request, response):
    token = request.GET.first(b'token')
    event = request.GET.first(b'event', None)

    with request.server.stash.lock:
        events = request.server.stash.take(token) or []
        if event is not None:
            events.append(event.decode())
        request.server.stash.put(token, events)

    return [(b'Cache-Control', b'no-store'), (b'Access-Control-Allow-Origin', b'*'), (b'Content-Type', b'text/plain')], u','.join(events)

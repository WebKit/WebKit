def _stash_dir(request):
    return u"/".join(request.url_parts.path.split(u"/")[:-1]) + u"/"


def main(request, response):
    # Non-blocking peek at whether `id` (a UUID) has been recorded by one of the recording
    # endpoints. Polled from the page rather than waited on server-side: blocking a request
    # handler on a side effect produced by another request to the same server holds a wptserve
    # thread and deadlocks under parallel/stress runs.
    ident = request.GET.first(b"id").decode()
    stash_dir = _stash_dir(request)
    value = request.server.stash.take(ident, stash_dir)
    if value is not None:
        # take() removes; put it back so this stays a peek.
        try:
            request.server.stash.put(ident, value, stash_dir)
        except Exception:
            pass

    headers = [
        (b"content-type", b"text/plain"),
        (b"cache-control", b"no-store"),
        (b"access-control-allow-origin", b"*"),
    ]
    return (200, u"OK"), headers, b"1" if value is not None else b"0"

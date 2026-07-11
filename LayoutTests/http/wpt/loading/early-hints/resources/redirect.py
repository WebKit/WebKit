def _stash_dir(request):
    return u"/".join(request.url_parts.path.split(u"/")[:-1]) + u"/"


def main(request, response):
    # `id` is a UUID (required for stash keys); it records that this redirect
    # endpoint was fetched (by the preload or the consumer).
    ident = request.GET.first(b"id").decode()
    redirect_to = request.GET.first(b"redirect_to")
    # The endpoint may be fetched more than once (preload + consumer); the stash is
    # write-once, so ignore a duplicate record.
    try:
        request.server.stash.put(ident, True, _stash_dir(request))
    except Exception:
        pass

    headers = [
        (b"location", redirect_to),
        (b"cache-control", b"no-store"),
    ]
    return (302, u"Found"), headers, b""

def _stash_dir(request):
    return u"/".join(request.url_parts.path.split(u"/")[:-1]) + u"/"


def main(request, response):
    # `id` is a UUID (required for stash keys); it records that this script was fetched.
    ident = request.GET.first(b"id").decode()
    # The endpoint may be fetched more than once (preload + consumer); the stash is
    # write-once, so ignore a duplicate record.
    try:
        request.server.stash.put(ident, True, _stash_dir(request))
    except Exception:
        pass

    headers = [
        (b"content-type", b"text/javascript"),
        (b"cache-control", b"no-store"),
    ]
    return (200, u"OK"), headers, b"window.scriptExecuted = true;"

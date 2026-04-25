ETAG = b'"123abc"'


def main(request, response):
    test_id = request.GET.first(b"uuid")

    if request.method == "POST":
        stashed_count = request.server.stash.take(test_id)
        if stashed_count is None:
            stashed_count = 0

        request.server.stash.put(test_id, stashed_count)

        response.status = (200, "OK")
        response.headers.set(b"Content-Type", b'text/plain')
        return str(stashed_count)

    stashed_count = request.server.stash.take(test_id)
    if stashed_count is not None:
        stashed_count = stashed_count + 1
    else:
        stashed_count = 1
    request.server.stash.put(test_id, stashed_count)

    etag = request.headers.get(b"If-None-Match", None)
    if etag == ETAG:
        response.headers.set(b"X-HTTP-STATUS", b"304")
        response.status = (304, "Not Modified")
        return ""

    # cache miss, so respond with the actual content
    response.status = (200, "OK")
    response.headers.set(b"ETag", ETAG)
    response.headers.set(b"Content-Type", b'image/png')
    response.headers.set(b"Cache-Control", b"max-age=0");
    return 'myimagecontent'

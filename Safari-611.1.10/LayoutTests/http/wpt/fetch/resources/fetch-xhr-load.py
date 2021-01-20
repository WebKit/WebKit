ETAG = '"123abc"'


def main(request, response):
    test_id = request.GET.first("uuid")
    stashed_count = request.server.stash.take(test_id)
    if stashed_count is not None:
        stashed_count = stashed_count + 1
    else:
        stashed_count = 0;

    request.server.stash.put(test_id, stashed_count)
    etag = request.headers.get("If-None-Match", None)
    if etag == ETAG:
        response.headers.set("X-HTTP-STATUS", 304)
        response.status = (304, "Not Modified")
        return ""

    # cache miss, so respond with the actual content
    response.status = (200, "OK")
    response.headers.set("ETag", ETAG)
    response.headers.set("Content-Type", 'text/plain')
    response.headers.set("Cache-Control", "max-age=0");
    return str(stashed_count)

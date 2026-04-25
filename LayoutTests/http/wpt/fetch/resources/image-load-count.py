def main(request, response):
    test_id = request.GET.first("uuid")
    stashed_count = request.server.stash.take(test_id)
    if stashed_count is None:
        stashed_count = test_id + " none"

    request.server.stash.put(test_id, stashed_count)

    response.status = (200, "OK")
    response.headers.set("Content-Type", 'text/plain')
    return str(stashed_count)

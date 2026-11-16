def main(request, response):
    response.headers.set(b"Access-Control-Allow-Origin", b"*")
    response.headers.set(b"Access-Control-Allow-Headers", b"If-Modified-Since")

    # Just return 304 for any request with If-Modified-Since.
    modifiedSince = request.headers.get(b"If-Modified-Since", None)
    if modifiedSince is not None:
        response.status = (304, "Not Modified")
        return ""

    # Otherwise return content from parameters.
    content = request.GET.first(b"content", None)
    mime = request.GET.first(b"mime", b"text/plain")
    date = request.GET.first(b"date", None)
    tao = request.GET.first(b"tao", None)

    if tao == b"true":
        response.headers.set(b"Timing-Allow-Origin", b"*")
    response.headers.set(b"Last-Modified", date)
    response.status = (200, "OK")
    response.headers.set(b"Content-Type", mime)
    return content

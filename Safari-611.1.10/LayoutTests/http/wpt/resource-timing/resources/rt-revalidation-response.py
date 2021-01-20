def main(request, response):
    response.headers.set("Access-Control-Allow-Origin", "*")
    response.headers.set("Access-Control-Allow-Headers", "If-Modified-Since")

    # Just return 304 for any request with If-Modified-Since.
    modifiedSince = request.headers.get("If-Modified-Since", None)
    if modifiedSince is not None:
        response.status = (304, "Not Modified")
        return ""

    # Otherwise return content from parameters.
    content = request.GET.first("content", None)
    mime = request.GET.first("mime", "text/plain")
    date = request.GET.first("date", None)
    tao = request.GET.first("tao", None)

    if tao == "true":
        response.headers.set("Timing-Allow-Origin", "*")
    response.headers.set("Last-Modified", date)
    response.status = (200, "OK")
    response.headers.set("Content-Type", mime)
    return content

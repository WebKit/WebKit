def main(request, response):
    headers = [(b"Content-Type", "text/html"),
               (b"Cache-Control", b"no-cache")]
    if b"authorization" in request.headers:
        return 200, headers, request.headers.get(b"Authorization")
    return 200, headers, "none"

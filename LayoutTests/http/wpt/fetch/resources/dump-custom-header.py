def main(request, response):
    headers = [(b"Content-Type", "text/html"),
               (b"Cache-Control", b"no-cache")]
    if b"custom-authorization" in request.headers:
        return 200, headers, request.headers.get(b"custom-authorization")
    return 200, headers, "none"

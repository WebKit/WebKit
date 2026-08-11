def main(request, response):
    origin = request.headers.get(b"Origin", "None")
    response.status = (200, "OK")
    response.headers.set(b"Content-Type", b'text/plain')
    response.headers.set(b"Cache-Control", b"max-age=0")
    return origin

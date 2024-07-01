def main(request, response):
    response.headers.set(b"Cache-Control", b"max-age=31536000")
    response.headers.set(b"Content-Type", b"text/html")
    return "<html><body></body></html>"

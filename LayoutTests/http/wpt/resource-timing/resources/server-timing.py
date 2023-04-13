def main(request, response):
    response.headers.set(b"Server-Timing", "my server timing")
    response.headers.set(b"Content-Type", "text/html")

    return "ok"

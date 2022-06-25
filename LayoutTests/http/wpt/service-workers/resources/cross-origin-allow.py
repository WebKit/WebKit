def main(request, response):
    value = request.headers.get("Cache-Control", "no cache-control header")
    return 200, [(b"Content-Type", b"text/ascii"), (b"Access-Control-Allow-Origin", "*")], value

def main(request, response):
    value = request.headers.get("Cache-Control", "no cache-control header")
    if b"pragma" in request.GET:
        value = request.headers.get("Pragma", "no pragma header")

    return 200, [(b"Content-Type", b"text/ascii"), (b"Access-Control-Allow-Origin", "*")], value

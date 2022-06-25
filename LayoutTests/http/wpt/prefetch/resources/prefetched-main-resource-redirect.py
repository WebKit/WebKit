def main(request, response):
    if b"prefetch" in request.headers.get(b"Purpose"):
        headers = [(b"Cache-Control", b"max-age=3600"), (b"Location", b"/WebKit/prefetch/resources/main-resource-redirect-no-prefetch.py")]
        return 302, headers, ""
    return 200, [], "FAIL"

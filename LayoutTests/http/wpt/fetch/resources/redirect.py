def main(request, response):
    headers = [(b"Cache-Control", b"no-cache"),
               (b"Pragma", b"no-cache"),
               (b"Location", request.GET[b'location'])]
    return 302, headers, ""

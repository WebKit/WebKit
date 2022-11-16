def main(request, response):
    headers = [(b"Location", request.GET[b'url']),
               (b"Cache-Control", b"public, max-age=86400")]
    return 301, headers, ""

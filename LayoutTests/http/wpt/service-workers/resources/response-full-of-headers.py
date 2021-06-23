def main(request, response):
    headers = [(b"Content-type", b"text/javascript"),
        (b"Set-Cookie", b"1"),
        (b"Set-Cookie2", b"2"),
        (b"Access-Control-Allow-Origin", b"*"),
        (b"Access-Control-Allow-Credentials", b"true"),
        (b"Access-Control-Allow-Methods", b"GET, POST, HEAD"),
        (b"Timing-Allow-Origin", b"*"),
        (b"Referrer-Policy", b"origin"),
        (b"SourceMap", b"1"),
        (b"x-sourcemap", b"2"),
        (b"Access-Control-Expose-Headers", b"x-Header1, content-length"),
        (b"x-header1", b"x-value1"),
        (b"x-header2", b"x-value2"),
        (b"Content-Length", b"13"),
        (b"Cache-Control", b"no-store")
    ]
    return headers, "document.body"

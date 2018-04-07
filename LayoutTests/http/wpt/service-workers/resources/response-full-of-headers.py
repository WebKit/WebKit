def main(request, response):
    headers = [("Content-type", "text/javascript"),
        ("Set-Cookie", "1"),
        ("Set-Cookie2", "2"),
        ("Access-Control-Allow-Origin", "*"),
        ("Access-Control-Allow-Credentials", "true"),
        ("Access-Control-Allow-Methods", "GET, POST, HEAD"),
        ("Timing-Allow-Origin", "*"),
        ("Referrer-Policy", "whatever"),
        ("SourceMap", "1"),
        ("x-sourcemap", "2"),
        ("Access-Control-Expose-Headers", "x-Header1, content-length"),
        ("x-header1", "x-value1"),
        ("x-header2", "x-value2"),
        ("Content-Length", "13"),
        ("Cache-Control", "no-store")
    ]
    return headers, "document.body"

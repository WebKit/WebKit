def main(request, response):
    if "prefetch" in request.headers.get("Purpose"):
        headers = [("Cache-Control", "max-age=3600"), ("Location", "/WebKit/prefetch/resources/main-resource-redirect-no-prefetch.py")]
        return 302, headers, ""
    return 200, [], "FAIL"

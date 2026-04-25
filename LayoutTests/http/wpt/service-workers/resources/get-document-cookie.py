def main(request, response):
    headers = [
        ("Content-Type", "text/html"),
    ]
    cookie = request.headers.get("Cookie", "no cookie")

    return headers, "<html><body><div>%s</div><script>if (window.testRunner) testRunner.notifyDone();</script></body></html>" % cookie.decode()

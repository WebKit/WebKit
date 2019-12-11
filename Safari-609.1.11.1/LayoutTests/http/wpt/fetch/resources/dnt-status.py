def main(request, response):
    headers = [("Content-Type", "text/plain"),
               ("Cache-Control", "no-cache"),
               ("Pragma", "no-cache")]

    if "store" in request.GET:
        request.server.stash.put(request.GET['token'], request.headers.get("DNT", "-1"))
        return 200, headers, ""

    return 200, headers, str(request.server.stash.take(request.GET['token']))

def main(request, response):
    headers = [("Content-Type", "text/html"), ("Set-Cookie", "foo=bar")]

    response.headers.set("Access-Control-Allow-Origin", "http://localhost:8800")
    response.headers.set("Access-Control-Allow-Credentials", "true")

    return headers, ""

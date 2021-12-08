def main(request, response):
    headers = [
        ("Content-Type", "text/ascii"),
    ]
    body = request.headers.get("Cookie", "")
    return headers, body

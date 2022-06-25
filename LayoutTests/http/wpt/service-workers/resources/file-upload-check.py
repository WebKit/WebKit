def main(request, response):

    return 200, [(b"Content-Type", b"text/html")], request.body

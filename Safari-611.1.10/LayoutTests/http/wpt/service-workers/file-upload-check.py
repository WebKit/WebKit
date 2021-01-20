def main(request, response):

    return 200, [("Content-Type", "text/html")], request.body

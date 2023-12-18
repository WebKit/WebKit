import random


def main(request, response):
    headers = [
        (b"Content-type", b"text/html"),
        (b"X-Frame-Options", b"SAMEORIGIN")
    ]
    return headers, "log('" + str(random.random()) + "');"

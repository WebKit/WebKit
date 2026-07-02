import random
import time


def main(request, response):
    headers = [
        (b"Content-type", b"text/javascript"),
        (b"Cache-Control", b"max-age=360000")
    ]
    time.sleep(0.5)
    return headers, "log('" + str(random.random()) + "');"

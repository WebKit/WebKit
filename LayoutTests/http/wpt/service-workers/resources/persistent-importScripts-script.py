import random

def main(request, response):
    headers = [(b"Content-type", b"text/javascript"),
        (b"Cache-Control", b"no-store")
    ]
    return headers, "self.addEventListener('message', function(e) { e.source.postMessage('" + str(random.random()) +"'); });"

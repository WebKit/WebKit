import random

def main(request, response):
    headers = [("Content-type", "text/javascript"),
        ("Cache-Control", "no-store")
    ]
    return headers, "self.addEventListener('message', function(e) { e.source.postMessage('" + str(random.random()) +"'); });"

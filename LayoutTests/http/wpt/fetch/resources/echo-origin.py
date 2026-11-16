def main(request, response):
    response.headers.set("Content-Type", "text/html")
    response.content = "<script>parent.postMessage('" + request.headers.get("Origin", "no header") + "')</script>"

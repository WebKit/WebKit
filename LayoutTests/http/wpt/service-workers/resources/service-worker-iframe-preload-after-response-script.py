def main(request, response):
    response.headers.set(b"Cache-Control", b"no-cache")
    response.headers.set(b"Content-Type", b"text/html")
    value = request.headers.get(b"Service-Worker-Navigation-Preload", b"NoPreload").decode("utf-8")
    return "<html><body><div>test</div><script>window.value = '" + value + "';</script></body></html>"

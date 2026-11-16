def main(request, response):

    service_worker_header_value = request.headers.get(b"Service-Worker")
    code = 200 if service_worker_header_value == b"script" else 404
    return code, [(b"Content-Type", b"application/javascript")], ""

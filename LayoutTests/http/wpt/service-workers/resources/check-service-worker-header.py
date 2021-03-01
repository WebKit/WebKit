def main(request, response):

    service_worker_header_value = request.headers.get("Service-Worker")
    code = 200 if service_worker_header_value == "script" else 404
    return code, [("Content-Type", "application/javascript")], ""

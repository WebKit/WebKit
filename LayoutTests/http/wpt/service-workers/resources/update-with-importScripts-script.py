def main(request, response):
    service_worker_header = request.headers.get(b'service-worker')

    headers = [(b'Cache-Control', b'no-cache, must-revalidate'),
               (b'Pragma', b'no-cache'),
               (b'Content-Type', b'application/javascript')]

    body = b'/* This is a service worker script */\n' if service_worker_header != b'script' else b'/* Bad Request */'
    return 200, headers, body

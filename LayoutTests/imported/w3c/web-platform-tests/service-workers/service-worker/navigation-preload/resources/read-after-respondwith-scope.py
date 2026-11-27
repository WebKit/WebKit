import time

def main(request, response):
    delay = request.GET.first(b'delay', b'')
    # A body load that falls back to a second network request gets a different body.
    if b'Service-Worker-Navigation-Preload' in request.headers:
        body = b'preload body'
    else:
        body = b'network body'

    if delay == b'headers':
        time.sleep(1)

    response.add_required_headers = False
    response.writer.write_status(200)
    response.writer.write_header(b'Content-Type', b'text/plain')
    response.writer.write_header(b'Content-Length', len(body))
    response.writer.write_header(b'Cache-Control', b'no-store')
    response.writer.end_headers()

    if delay == b'body':
        time.sleep(1)

    response.writer.write(body)

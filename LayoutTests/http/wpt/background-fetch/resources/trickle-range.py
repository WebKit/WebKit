import re
import time

from wptserve.utils import isomorphic_decode


def main(request, response):
    delay = float(request.GET.first(b"ms", 500)) / 1E3
    count = int(request.GET.first(b"count", 50))
    range_header = request.headers.get(b'Range', b'')

    start = 0
    total_length = count
    if not range_header:
        start = 0
        response.status = 200
    else:
        result = re.findall(r'\d+', isomorphic_decode(range_header))
        start = int(result[0])
        response.status = 206

    length = total_length - start

    response.headers.set(b"Accept-Ranges", b"bytes")

    if length != total_length:
        content_range = b"bytes %d-%d/%d" % (start, total_length - 1, total_length)
        response.headers.set(b"Content-Range", content_range)

    response.headers.set(b"Content-Length", length)
    response.headers.set(b"Content-type", b"text/plain")

    # Read request body
    time.sleep(delay)
    response.write_status_headers()

    for i in range(length):
        response.writer.write_content(b"T")
        time.sleep(delay)

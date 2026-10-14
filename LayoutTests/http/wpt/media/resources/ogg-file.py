import os.path
import re

from wptserve.utils import isomorphic_decode

def main(request, response):
  contentType = request.GET.first(b"contentType", b"")
  path = os.path.join(os.path.dirname(isomorphic_decode(__file__)), u"silence.ogg")
  body = open(path, "rb").read()

  response.add_required_headers = False

  range_header = request.headers.get(b'Range', b'')
  range_header_match = range_header and re.search(r'^bytes=(\d*)-(\d*)$', isomorphic_decode(range_header))

  if range_header_match:
    response.writer.write_status(206)
    response.writer.write_header(b"content-type", path)
    response.writer.write_header(b"Accept-Ranges", b"bytes")

    total_length = len(body)
    start, end = range_header_match.groups()

    start = int(start)
    end = int(end) if end else total_length - 1

    content_range = b"bytes %d-%d/%d" % (start, end, total_length)
    response.writer.write_header(b"Content-Range", content_range)
    response.writer.write_header(b"Content-Length", end + 1 - start)

    response.writer.end_headers()
    response.writer.write(body)
  else:
    response.writer.write_status(200)
    response.writer.write_header(b"content-length", len(body))
    response.writer.write_header(b"content-type", path)
    response.writer.write_header(b"Accept-Ranges", b"bytes")
    response.writer.end_headers()
    response.writer.write(body)

import os.path

from wptserve.utils import isomorphic_decode

def main(request, response):
  contentType = request.GET.first(b"contentType", b"")
  path = os.path.join(os.path.dirname(isomorphic_decode(__file__)), u"silence.ogg")
  body = open(path, "rb").read()

  response.add_required_headers = False
  response.writer.write_status(200)
  response.writer.write_header(b"content-length", len(body))
  response.writer.write_header(b"content-type", path)
  response.writer.end_headers()

  response.writer.write(body)

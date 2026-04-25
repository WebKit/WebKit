import os.path

from wptserve.utils import isomorphic_decode


def main(request, response):
    body = open(os.path.join(os.path.dirname(isomorphic_decode(__file__)), u"green.png"), u"rb").read()

    response.add_required_headers = False
    response.writer.write_status(200)

    response.writer.write_header(b"access-control-allow-origin", b"*")
    response.writer.write_header(b"content-length", len(body))
    response.writer.write_header(b"content-type", b"image/png")
    response.writer.end_headers()

    response.writer.write(body)

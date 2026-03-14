from wptserve.utils import isomorphic_decode


def main(request, response):
    value = request.GET.first(b"value", b"")
    body = b"--boundary" + value + b"--boundary--\r\n"
    headers = [(b"Content-Type", b"multipart/form-data; boundary=boundary"),
               (b"Cache-Control", b"no-cache"),
               (b"Pragma", b"no-cache")]

    return 200, headers, body

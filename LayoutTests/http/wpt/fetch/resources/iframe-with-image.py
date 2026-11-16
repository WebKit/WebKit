from wptserve.utils import isomorphic_decode

ETAG = b'"123abc"'


def main(request, response):
    test_id = isomorphic_decode(request.GET.first(b"uuid"))
    response.status = (200, "OK")
    response.headers.set(b"Content-Type", b'text/html')
    return "<!doctype html><image src='image-load.py?uuid=" + test_id + "'></image>"

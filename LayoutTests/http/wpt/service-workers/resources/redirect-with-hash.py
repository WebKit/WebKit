from wptserve.utils import isomorphic_decode


def main(request, response):
    headers = []
    headers.append((b"Location", b"/redirected"))

    return 302, headers, b""

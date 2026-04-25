from wptserve.utils import isomorphic_decode


def main(request, response):
    headers = []
    headers.append((b"Location", b"/WebKit/service-workers/resources/cached-after-redirection.html"))

    return 302, headers, b""

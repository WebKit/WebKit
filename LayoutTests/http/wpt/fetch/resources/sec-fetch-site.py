import os
import hashlib
import json

from wptserve.utils import isomorphic_decode


def main(request, response):
    secFetchSite = request.headers.get(b"sec-fetch-site", b"")
    response.headers.set(b"Content-Type", b"text/html")
    return b"<html><script>window.parent.postMessage('" + secFetchSite + b"', '*');</script></html>"

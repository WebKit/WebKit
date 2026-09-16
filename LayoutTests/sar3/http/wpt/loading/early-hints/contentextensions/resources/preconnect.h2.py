import os
from urllib.parse import quote

from wptserve.utils import isomorphic_encode

SCRIPT_DIR = os.path.dirname(__file__)


def handle_headers(frame, request, response):
    preconnectOrigin = request.GET.first(b"preconnect_origin").decode()

    if b"redirect_from" in request.GET:
        # Redirect the main-frame navigation to the same script without redirect_from, so the
        # post-redirect URL (response.url()) differs from the loader's original request URL. This
        # drives which URL the network-process content-rule-list check treats as the main-document
        # URL for the 103 early-hints preconnect. See the sibling preconnect-redirect.h2.window.js.
        location = "preconnect.h2.py?preconnect_origin=" + quote(preconnectOrigin, safe="")
        response.status = 301
        response.headers[b"location"] = isomorphic_encode(location)
        response.write_status_headers()
        return

    earlyHeaders = [
        (b":status", b"103"),
        (b"link", f"<{preconnectOrigin}>; rel=preconnect"),
    ]

    response.writer.write_raw_header_frame(headers=earlyHeaders, end_headers=True)

    response.status = 200
    response.headers["content-type"] = "text/html"
    response.headers["timing-allow-origin"] = "*"
    response.write_status_headers()


def main(request, response):
    if b"redirect_from" in request.GET:
        return
    with open(os.path.join(SCRIPT_DIR, "preconnect.html"), "r") as f:
        response.writer.write_data(item=f.read(), last=True)

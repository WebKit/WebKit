import os

SCRIPT_DIR = os.path.dirname(__file__)


def handle_headers(frame, request, response):
    preconnectOrigin = request.GET.first(b"preconnect_origin").decode()
    earlyHeaders = [
        (b":status", b"103"),
        (b"link", f"<{preconnectOrigin}>; rel=preconnect"),
    ]

    csp = request.GET.first(b"csp", b"").decode()
    if csp == 'allow-all':
        earlyHeaders.append((b"content-security-policy", b'default-src *'))
    elif csp == 'allow-self':
        earlyHeaders.append((b"content-security-policy", b"default-src 'self'"))

    response.writer.write_raw_header_frame(headers=earlyHeaders, end_headers=True)

    response.status = 200
    response.headers["content-type"] = "text/html"
    response.headers["timing-allow-origin"] = "*"
    response.write_status_headers()


def main(request, response):
    with open(os.path.join(SCRIPT_DIR, "preconnect.html"), "r") as f:
        response.writer.write_data(item=f.read(), last=True)

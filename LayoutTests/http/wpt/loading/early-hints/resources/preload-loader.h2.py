import os
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def _preload_path(request):
    return request.GET.first(b"preload_path").decode()


def handle_headers(frame, request, response):
    preload_path = _preload_path(request)
    early_hints = [
        (b":status", b"103"),
        (b"link", "<{}>; rel=preload; as=script".format(preload_path).encode()),
    ]
    response.writer.write_raw_header_frame(headers=early_hints, end_headers=True)

    # Give the preload a head start, as the upstream early hints loader does. Tests that need the
    # preload to have actually completed poll status.py from the page instead: blocking here would
    # hold a wptserve thread while the preload needs that same server, which deadlocks under stress.
    time.sleep(0.2)

    response.status = 200
    response.headers[b"content-type"] = b"text/html"
    response.headers[b"timing-allow-origin"] = b"*"
    response.write_status_headers()


def main(request, response):
    with open(os.path.join(SCRIPT_DIR, "preload-test.html"), "r") as f:
        html = f.read()
    response.writer.write_data(item=html.replace("PRELOAD_URL_PLACEHOLDER", _preload_path(request)), last=True)

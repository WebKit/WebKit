import time


def main(request, response):
    response.headers.set(b"Content-Type", b"application/octet-stream")
    response.write_status_headers()
    response.writer.write_content(b"A" * 16)
    # Deliver the second chunk in a separate network callback, after the
    # controller has stopped pulling, then go quiet without closing.
    time.sleep(0.5)
    response.writer.write_content(b"B" * 16)
    time.sleep(8)

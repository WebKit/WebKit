#!/usr/bin/env python3
import os
import tempfile


def main(request, response):
    filename = request.GET[b'filename']
    state_file = os.path.join(tempfile.gettempdir(), filename.decode())
    if os.path.exists(state_file):
        os.remove(state_file)
    return 200, [(b"Content-Type", b"text/html")], ""

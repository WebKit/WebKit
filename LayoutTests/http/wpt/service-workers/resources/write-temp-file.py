#!/usr/bin/env python3

import os
import sys
import tempfile


def main(request, response):
    filename = request.GET[b'filename']
    data = request.GET[b'data']

    tmp_file = os.path.join(tempfile.gettempdir(), filename.decode())
    with open(tmp_file, 'w') as open_file:
        open_file.write(data.decode())

    return 200, [(b"Content-Type", b"text/html")], tmp_file

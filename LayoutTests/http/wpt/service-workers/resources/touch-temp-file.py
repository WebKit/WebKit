#!/usr/bin/env python3

import os
import tempfile


def main(request, response):
    filename = request.GET[b'filename']
    tmpFilePath = os.path.join(tempfile.gettempdir(), filename.decode())
    stat = os.stat(tmpFilePath)

    if not stat:
        return 200, [(b"Content-Type", b"text/html")], "FAIL: stat() call failed.\n"

    atime = stat.st_atime
    mtime = stat.st_mtime
    try:
        os.utime(tmpFilePath, times=(atime, mtime + 1))
    except:
        return 200, [(b"Content-Type", b"text/html")], "FAIL: touch() call failed.\n"
    return 200, [(b"Content-Type", b"text/html")], ""

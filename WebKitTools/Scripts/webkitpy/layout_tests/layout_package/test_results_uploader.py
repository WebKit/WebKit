#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import mimetypes
import socket

from webkitpy.common.net.networktransaction import NetworkTransaction
from webkitpy.thirdparty.autoinstalled.mechanize import Browser


def get_mime_type(filename):
    return mimetypes.guess_type(filename)[0] or "text/plain"


class TestResultsUploader:
    def __init__(self, host):
        self._host = host
        self._browser = Browser()

    def _upload_files(self, attrs, file_objs):
        self._browser.open("http://%s/testfile/uploadform" % self._host)
        self._browser.select_form("test_result_upload")
        for (name, data) in attrs:
            self._browser[name] = str(data)

        for (filename, handle) in file_objs:
            self._browser.add_file(handle, get_mime_type(filename), filename,
                "file")

        self._browser.submit()

    def upload(self, params, files, timeout_seconds):
        orig_timeout = socket.getdefaulttimeout()
        file_objs = []
        try:
            file_objs = [(filename, open(path, "rb")) for (filename, path)
                in files]

            socket.setdefaulttimeout(timeout_seconds)
            NetworkTransaction(timeout_seconds=timeout_seconds).run(
                lambda: self._upload_files(params, file_objs))
        finally:
            socket.setdefaulttimeout(orig_timeout)
            for (filename, handle) in file_objs:
                handle.close()

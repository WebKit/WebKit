# Copyright (C) 2019 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import requests
import socket
import time

from flask import Flask
from multiprocessing import Process, Semaphore

_ORIGINAL_CREATE_CONNECTION = socket.create_connection
_LOCALHOST_ADDRESSES = frozenset(('localhost', '127.0.0.1', '::1'))


def _block_external_connections():
    def _localhost_only(address, *args, **kwargs):
        host = address[0] if isinstance(address, tuple) else address
        if host in _LOCALHOST_ADDRESSES:
            return _ORIGINAL_CREATE_CONNECTION(address, *args, **kwargs)
        raise ConnectionRefusedError(
            f'Test server blocked connection to external host: {host}'
        )
    socket.create_connection = _localhost_only


class FlaskTestContext(object):
    PORT = 5001

    @staticmethod
    def start_webserver(cls, semaphore, **kwargs):
        _block_external_connections()
        try:
            app = Flask('testing')
            cls.setup_webserver(app, **kwargs)
            app.add_url_rule('/__health', 'health', lambda: 'ok', methods=('GET',))
        finally:
            semaphore.release()
        return app.run(host='0.0.0.0', port=FlaskTestContext.PORT)

    def __init__(self, cls, **kwargs):
        self.cls = cls
        self.kwargs = kwargs
        self.process = None

    def __enter__(self):
        semaphore = Semaphore(0)
        self.process = Process(target=self.start_webserver, args=(self.cls, semaphore), kwargs=self.kwargs)
        self.process.start()

        with semaphore:
            for attempt in range(3):
                if not self.process.is_alive():
                    raise RuntimeError('Exception raised when starting web-server')

                try:
                    response = requests.get(f'http://localhost:{self.PORT}/__health')
                    if response.text != 'ok':
                        raise RuntimeError('Health check failed')
                    return
                except requests.ConnectionError as e:
                    time.sleep(.05)

        raise RuntimeError('Failed to connect to server for health check')

    def __exit__(self, *args):
        if not self.process:
            return
        if not self.process.is_alive():
            raise RuntimeError('Web-server has crashed')
        self.process.terminate()

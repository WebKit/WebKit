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
import time

from flask import Flask
from multiprocessing import Process, Semaphore


class FlaskTestContext(object):
    PORT = 5001

    @classmethod
    def start_webserver(cls, method, semaphore):
        try:
            app = Flask('testing')
            method(app)
            app.add_url_rule('/__health', 'health', lambda: 'ok', methods=('GET',))
        finally:
            semaphore.release()
        return app.run(host='0.0.0.0', port=cls.PORT)

    def __init__(self, method):
        self.method = method
        self.process = None

    def __enter__(self):
        semaphore = Semaphore(0)
        self.process = Process(target=self.start_webserver, args=(self.method, semaphore))
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

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

import random
import requests
import threading
import time

from redis import StrictRedis


class CasseroleNodes(object):
    DEFAULT_INTERVAL = 120  # Refresh every 2 minutes

    def __init__(self, url, interval_seconds=DEFAULT_INTERVAL, asynchronous=True):
        self.url = url
        self.interval = interval_seconds
        self._epoch = 0
        self.asynchronous = asynchronous
        self._nodes = []
        self.retrieve_nodes()
        if not self._nodes:
            raise RuntimeError(f'Cannot communicate with Casserole url {self.url}')

    def retrieve_nodes(self):
        casserole_response = requests.get(self.url)
        if casserole_response.status_code != 200:
            return
        self._epoch = time.time()
        self._nodes = casserole_response.text.split(',')

    @property
    def nodes(self):
        if self._epoch + self.interval > time.time():
            return self._nodes

        if self.asynchronous:
            request_thread = threading.Thread(target=self.retrieve_nodes)
            request_thread.daemon = True
            request_thread.start()
        else:
            self.retrieve_nodes()
        return self._nodes

    def __len__(self):
        return len(self.nodes)

    def __iter__(self):
        return iter(self.nodes)


class CasseroleRedis(object):
    DEFAULT_INTERVAL = 120  # Refresh every 2 minutes

    def __init__(
        self, url, port=6379, password=None, interval_seconds=DEFAULT_INTERVAL, asynchronous=True,
        ssl_keyfile=None, ssl_certfile=None, ssl_cert_reqs='required', ssl_ca_certs=None,
    ):
        self.url = url
        self.interval = interval_seconds
        self.asynchronous = asynchronous

        self._redis = None
        self._redis_url = None
        self._redis_kwargs = dict(
            port=port,
            password=password,
            ssl_keyfile=ssl_keyfile,
            ssl_certfile=ssl_certfile,
            ssl_cert_reqs=ssl_cert_reqs,
            ssl_ca_certs=ssl_ca_certs,
        )

        self._epoch = time.time()
        self.connect()

    def connect(self):
        if self._redis and self._epoch + self.interval > time.time():
            return

        def do_connection(obj=self):
            casserole_response = requests.get(obj.url)
            if casserole_response.status_code != 200:
                return

            candidates = {}
            for candidate in casserole_response.json():
                if not candidate['isMaster']:
                    continue
                candidates[candidate['host']] = candidate
            if self._redis and self._redis_url in candidates:
                # We're still connected, we don't need to do anything
                return

            self._redis_url = random.choice(list(candidates.keys()))
            port = candidates[self._redis_url].get('sslPort')
            kwargs = dict(**self._redis_kwargs)
            kwargs['port'] = port or kwargs['port']

            self._redis = StrictRedis(
                host=self._redis_url,
                ssl=True if port else False,
                **kwargs
            )

            obj._epoch = time.time()

        if self.asynchronous and self._redis:
            request_thread = threading.Thread(target=do_connection)
            request_thread.daemon = True
            request_thread.start()
        else:
            do_connection()

    def ping(self):
        self.connect()
        return self._redis.ping()

    def get(self, name):
        self.connect()
        return self._redis.get(name)

    def set(self, *args, **kwargs):
        self.connect()
        return self._redis.set(*args, **kwargs)

    def lock(self, name, **kwargs):
        self.connect()
        return self._redis.lock(name, **kwargs)

    def delete(self, *names):
        self.connect()
        return self._redis.delete(*names)

    def scan_iter(self, match=None, **kwargs):
        self.connect()
        return self._redis.scan_iter(match=match, **kwargs)

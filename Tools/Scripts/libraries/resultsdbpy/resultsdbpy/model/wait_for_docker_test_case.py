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

import os
import sys
import unittest

from fakeredis import FakeStrictRedis
from redis import StrictRedis
from resultsdbpy.model.docker import Docker


class WaitForDockerTestCase(unittest.TestCase):

    def setUp(self):
        if Docker.is_running() and int(os.environ.get('slow_tests', '0')):
            with Docker.instance():
                StrictRedis().flushdb()
        FakeStrictRedis().flushdb()

    @classmethod
    def combine(cls, *args):
        def decorator(func):
            for elm in reversed(args):
                func = elm(func)
            return func

        return decorator

    @classmethod
    def run_if_slow(cls):
        return unittest.skipIf(not int(os.environ.get('slow_tests', '0')), 'Slow tests disabled')

    @classmethod
    def run_if_has_docker(cls):
        return cls.combine(cls.run_if_slow(), unittest.skipIf(not Docker.installed(), 'Docker not installed'))

    @classmethod
    def mock_if_no_docker(cls, **kwargs):
        mock_args = {}
        for key, value in kwargs.items():
            if not key.startswith('mock_'):
                raise Exception('{} does not start with \'mock_\' and is an invalid argument')
            mock_args[key[len('mock_'):]] = value

        def decorator(method, mock_args=mock_args):
            # Use frame introspection to create a method in the containing frame.
            frame = sys._getframe(1)  # pylint: disable-msg=W0212
            frame.f_locals[method.__name__ + '_mock'] = lambda val: method(val, **mock_args)

            def real_method(val, method=method):
                with Docker.instance():
                    return method(val)
            return cls.run_if_has_docker()(real_method)

        return decorator

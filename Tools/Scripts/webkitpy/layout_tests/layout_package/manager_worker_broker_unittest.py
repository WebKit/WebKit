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

import optparse
import sys
import unittest

try:
    import multiprocessing
except ImportError:
    multiprocessing = None


from webkitpy.common.system import outputcapture

from webkitpy.layout_tests import port
from webkitpy.layout_tests.layout_package import manager_worker_broker
from webkitpy.layout_tests.layout_package import message_broker2


class TestWorker(object):
    pass


def get_options(worker_model):
    option_list = manager_worker_broker.runtime_options()
    parser = optparse.OptionParser(option_list=option_list)
    options, args = parser.parse_args(args=['--worker-model', worker_model])
    return options


def make_broker(manager, worker_model):
    options = get_options(worker_model)
    return manager_worker_broker.get(port.get("test"), options, manager,
                                     TestWorker)


class FunctionTests(unittest.TestCase):
    def test_get__inline(self):
        self.assertTrue(make_broker(self, 'inline') is not None)

    def test_get__threads(self):
        self.assertRaises(NotImplementedError, make_broker, self, 'threads')

    def test_get__processes(self):
        if multiprocessing:
            self.assertRaises(NotImplementedError, make_broker, self, 'processes')
        else:
            self.assertRaises(ValueError, make_broker, self, 'processes')

    def test_get__unknown(self):
        self.assertRaises(ValueError, make_broker, self, 'unknown')

    def test_runtime_options(self):
        option_list = manager_worker_broker.runtime_options()
        parser = optparse.OptionParser(option_list=option_list)
        options, args = parser.parse_args([])
        self.assertTrue(options)

# FIXME: Add in unit tests for the managers, coverage tests for the interfaces.


if __name__ == '__main__':
    unittest.main()

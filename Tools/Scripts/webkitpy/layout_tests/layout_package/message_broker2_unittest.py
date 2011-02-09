# Copyright (C) 2011 Google Inc. All rights reserved.
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

import unittest

from webkitpy.layout_tests.layout_package import message_broker2

# This file exists to test routines that aren't necessarily covered elsewhere;
# most of the testing of message_broker2 will be covered under the tests in
# the manager_worker_broker module.


class MessageTest(unittest.TestCase):
    def test__no_body(self):
        msg = message_broker2._Message('src', 'topic_name', 'message_name', None)
        self.assertTrue(repr(msg))
        s = msg.dumps()
        new_msg = message_broker2._Message.loads(s)
        self.assertEqual(new_msg.name, 'message_name')
        self.assertEqual(new_msg.args, None)
        self.assertEqual(new_msg.topic_name, 'topic_name')
        self.assertEqual(new_msg.src, 'src')

    def test__body(self):
        msg = message_broker2._Message('src', 'topic_name', 'message_name',
                                      ('body', 0))
        self.assertTrue(repr(msg))
        s = msg.dumps()
        new_msg = message_broker2._Message.loads(s)
        self.assertEqual(new_msg.name, 'message_name')
        self.assertEqual(new_msg.args, ('body', 0))
        self.assertEqual(new_msg.topic_name, 'topic_name')
        self.assertEqual(new_msg.src, 'src')


class InterfaceTest(unittest.TestCase):
    # These tests mostly exist to pacify coverage.

    # FIXME: There must be a better way to do this and also verify
    # that classes do implement every abstract method in an interface.

    def test_brokerclient_is_abstract(self):
        # Test that we can't create an instance directly.
        self.assertRaises(NotImplementedError, message_broker2.BrokerClient)

        class TestClient(message_broker2.BrokerClient):
            def __init__(self):
                pass

        # Test that all the base class methods are abstract and have the
        # signature we expect.
        obj = TestClient()
        self.assertRaises(NotImplementedError, obj.is_done)
        self.assertRaises(NotImplementedError, obj.name)


if __name__ == '__main__':
    unittest.main()

# Copyright (C) 2010 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest
from StringIO import StringIO

import messages

_messages_file_contents = """# Copyright (C) 2010 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

messages -> WebPage {
    LoadURL(WTF::String url)
#if ENABLE(TOUCH_EVENTS)
    TouchEvent(WebKit::WebTouchEvent event)
#endif
    DidReceivePolicyDecision(uint64_t frameID, uint64_t listenerID, uint32_t policyAction)
    Close()
}
"""

_expected_results = {
    'name': 'WebPage',
    'messages': (
        {
            'name': 'LoadURL',
            'parameters': (
                ('WTF::String', 'url'),
            ),
            'condition': None,
            'base_class': 'CoreIPC::Arguments1<const WTF::String&>',
        },
        {
            'name': 'TouchEvent',
            'parameters': (
                ('WebKit::WebTouchEvent', 'event'),
            ),
            'condition': 'ENABLE(TOUCH_EVENTS)',
            'base_class': 'CoreIPC::Arguments1<const WebKit::WebTouchEvent&>',
        },
        {
            'name': 'DidReceivePolicyDecision',
            'parameters': (
                ('uint64_t', 'frameID'),
                ('uint64_t', 'listenerID'),
                ('uint32_t', 'policyAction'),
            ),
            'condition': None,
            'base_class': 'CoreIPC::Arguments3<uint64_t, uint64_t, uint32_t>',
        },
        {
            'name': 'Close',
            'parameters': (),
            'condition': None,
            'base_class': 'CoreIPC::Arguments0',
        },
    ),
}


class MessagesTest(unittest.TestCase):
    def setUp(self):
        self.receiver = messages.MessageReceiver.parse(StringIO(_messages_file_contents))


class ParsingTest(MessagesTest):
    def check_message(self, message, expected_message):
        self.assertEquals(message.name, expected_message['name'])
        self.assertEquals(len(message.parameters), len(expected_message['parameters']))
        for index, parameter in enumerate(message.parameters):
            self.assertEquals(parameter.type, expected_message['parameters'][index][0])
            self.assertEquals(parameter.name, expected_message['parameters'][index][1])
        self.assertEquals(message.condition, expected_message['condition'])

    def test_receiver(self):
        """Receiver should be parsed as expected"""
        self.assertEquals(self.receiver.name, _expected_results['name'])
        self.assertEquals(len(self.receiver.messages), len(_expected_results['messages']))
        for index, message in enumerate(self.receiver.messages):
            self.check_message(message, _expected_results['messages'][index])


class HeaderTest(MessagesTest):
    def test_base_class(self):
        """Base classes for message structs should match expectations"""
        for index, message in enumerate(self.receiver.messages):
            self.assertEquals(messages.base_class(message), _expected_results['messages'][index]['base_class'])

class ReceiverImplementationTest(unittest.TestCase):
    def setUp(self):
        self.file_contents = messages.generate_message_handler(StringIO(_messages_file_contents))

    def test_receiver_header_listed_first(self):
        """Receiver's header file should be the first included header"""
        self.assertEquals(self.file_contents.index('#include'), self.file_contents.index('#include "%s.h"' % _expected_results['name']))

    def test_receiver_header_listed_separately(self):
        """Receiver's header should be in its own paragraph"""
        self.assertTrue(self.file_contents.find('\n\n#include "%s.h"\n\n' % _expected_results['name']))


if __name__ == '__main__':
    unittest.main()

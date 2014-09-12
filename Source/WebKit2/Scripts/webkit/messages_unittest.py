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

import os
import unittest
from StringIO import StringIO

import messages
import parser

print os.getcwd()

script_directory = os.path.dirname(os.path.realpath(__file__))

with open(os.path.join(script_directory, 'test-messages.in')) as file:
    _messages_file_contents = file.read()

with open(os.path.join(script_directory, 'test-legacy-messages.in')) as file:
    _legacy_messages_file_contents = file.read()

with open(os.path.join(script_directory, 'test-superclass-messages.in')) as file:
    _superclass_messages_file_contents = file.read()


with open(os.path.join(script_directory, 'Messages-expected.h')) as file:
    _expected_receiver_header = file.read()

with open(os.path.join(script_directory, 'LegacyMessages-expected.h')) as file:
    _expected_legacy_receiver_header = file.read()

with open(os.path.join(script_directory, 'MessagesSuperclass-expected.h')) as file:
    _expected_superclass_receiver_header = file.read()
    

with open(os.path.join(script_directory, 'MessageReceiver-expected.cpp')) as file:
    _expected_receiver_implementation = file.read()

with open(os.path.join(script_directory, 'LegacyMessageReceiver-expected.cpp')) as file:
    _expected_legacy_receiver_implementation = file.read()

with open(os.path.join(script_directory, 'MessageReceiverSuperclass-expected.cpp')) as file:
    _expected_superclass_receiver_implementation = file.read()

_expected_results = {
    'name': 'WebPage',
    'conditions': ('(ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))'),
    'messages': (
        {
            'name': 'LoadURL',
            'parameters': (
                ('String', 'url'),
            ),
            'conditions': (None),
        },
        {
            'name': 'LoadSomething',
            'parameters': (
                ('String', 'url'),
            ),
            'conditions': ('ENABLE(TOUCH_EVENTS)'),
        },
        {
            'name': 'TouchEvent',
            'parameters': (
                ('WebKit::WebTouchEvent', 'event'),
            ),
            'conditions': ('(ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))'),
        },
        {
            'name': 'AddEvent',
            'parameters': (
                ('WebKit::WebTouchEvent', 'event'),
            ),
            'conditions': ('(ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))'),
        },
        {
            'name': 'LoadSomethingElse',
            'parameters': (
                ('String', 'url'),
            ),
            'conditions': ('ENABLE(TOUCH_EVENTS)'),
        },
        {
            'name': 'DidReceivePolicyDecision',
            'parameters': (
                ('uint64_t', 'frameID'),
                ('uint64_t', 'listenerID'),
                ('uint32_t', 'policyAction'),
            ),
            'conditions': (None),
        },
        {
            'name': 'Close',
            'parameters': (),
            'conditions': (None),
        },
        {
            'name': 'PreferencesDidChange',
            'parameters': (
                ('WebKit::WebPreferencesStore', 'store'),
            ),
            'conditions': (None),
        },
        {
            'name': 'SendDoubleAndFloat',
            'parameters': (
                ('double', 'd'),
                ('float', 'f'),
            ),
            'conditions': (None),
        },
        {
            'name': 'SendInts',
            'parameters': (
                ('Vector<uint64_t>', 'ints'),
                ('Vector<Vector<uint64_t>>', 'intVectors')
            ),
            'conditions': (None),
        },
        {
            'name': 'CreatePlugin',
            'parameters': (
                ('uint64_t', 'pluginInstanceID'),
                ('WebKit::Plugin::Parameters', 'parameters')
            ),
            'reply_parameters': (
                ('bool', 'result'),
            ),
            'conditions': (None),
        },
        {
            'name': 'RunJavaScriptAlert',
            'parameters': (
                ('uint64_t', 'frameID'),
                ('String', 'message')
            ),
            'reply_parameters': (),
            'conditions': (None),
        },
        {
            'name': 'GetPlugins',
            'parameters': (
                ('bool', 'refresh'),
            ),
            'reply_parameters': (
                ('Vector<WebCore::PluginInfo>', 'plugins'),
            ),
            'conditions': (None),
        },
        {
            'name': 'GetPluginProcessConnection',
            'parameters': (
                ('String', 'pluginPath'),
            ),
            'reply_parameters': (
                ('IPC::Connection::Handle', 'connectionHandle'),
            ),
            'conditions': (None),
        },
        {
            'name': 'TestMultipleAttributes',
            'parameters': (
            ),
            'reply_parameters': (
            ),
            'conditions': (None),
        },
        {
            'name': 'TestParameterAttributes',
            'parameters': (
                ('uint64_t', 'foo', ('AttributeOne', 'AttributeTwo')),
                ('double', 'bar'),
                ('double', 'baz', ('AttributeThree',)),
            ),
            'conditions': (None),
        },
        {
            'name': 'TemplateTest',
            'parameters': (
                ('HashMap<String, std::pair<String, uint64_t>>', 'a'),
            ),
            'conditions': (None),
        },
        {
            'name': 'SetVideoLayerID',
            'parameters': (
                ('WebCore::GraphicsLayer::PlatformLayerID', 'videoLayerID'),
            ),
            'conditions': (None),
        },
        {
            'name': 'DidCreateWebProcessConnection',
            'parameters': (
                ('IPC::MachPort', 'connectionIdentifier'),
            ),
            'conditions': ('PLATFORM(MAC)'),
        },
        {
            'name': 'InterpretKeyEvent',
            'parameters': (
                ('uint32_t', 'type'),
            ),
            'reply_parameters': (
                ('Vector<WebCore::KeypressCommand>', 'commandName'),
            ),
            'conditions': ('PLATFORM(MAC)'),
        },
        {
            'name': 'DeprecatedOperation',
            'parameters': (
                ('IPC::DummyType', 'dummy'),
            ),
            'conditions': ('ENABLE(DEPRECATED_FEATURE)'),
        },
        {
            'name': 'ExperimentalOperation',
            'parameters': (
                ('IPC::DummyType', 'dummy'),
            ),
            'conditions': ('ENABLE(EXPERIMENTAL_FEATURE)'),
        }
    ),
}

_expected_superclass_results = {
    'name': 'WebPage',
    'superclass' : 'WebPageBase',
    'conditions': None,
    'messages': (
        {
            'name': 'LoadURL',
            'parameters': (
                ('String', 'url'),
            ),
            'conditions': (None),
        },
    ),
}


class MessagesTest(unittest.TestCase):
    def setUp(self):
        self.receiver = parser.parse(StringIO(_messages_file_contents))
        self.legacy_receiver = parser.parse(StringIO(_legacy_messages_file_contents))
        self.superclass_receiver = parser.parse(StringIO(_superclass_messages_file_contents))


class ParsingTest(MessagesTest):
    def check_message(self, message, expected_message):
        self.assertEquals(message.name, expected_message['name'])
        self.assertEquals(len(message.parameters), len(expected_message['parameters']))
        for index, parameter in enumerate(message.parameters):
            expected_parameter = expected_message['parameters'][index]
            self.assertEquals(parameter.type, expected_parameter[0])
            self.assertEquals(parameter.name, expected_parameter[1])
            if len(expected_parameter) > 2:
                self.assertEquals(parameter.attributes, frozenset(expected_parameter[2]))
                for attribute in expected_parameter[2]:
                    self.assertTrue(parameter.has_attribute(attribute))
            else:
                self.assertEquals(parameter.attributes, frozenset())
        if message.reply_parameters != None:
            for index, parameter in enumerate(message.reply_parameters):
                self.assertEquals(parameter.type, expected_message['reply_parameters'][index][0])
                self.assertEquals(parameter.name, expected_message['reply_parameters'][index][1])
        else:
            self.assertFalse('reply_parameters' in expected_message)
        self.assertEquals(message.condition, expected_message['conditions'])

    def test_receiver(self):
        """Receiver should be parsed as expected"""
        self.assertEquals(self.receiver.name, _expected_results['name'])
        self.assertEquals(self.receiver.condition, _expected_results['conditions'])
        self.assertEquals(len(self.receiver.messages), len(_expected_results['messages']))
        for index, message in enumerate(self.receiver.messages):
            self.check_message(message, _expected_results['messages'][index])

        self.assertEquals(self.legacy_receiver.name, _expected_results['name'])
        self.assertEquals(self.legacy_receiver.condition, _expected_results['conditions'])
        self.assertEquals(len(self.legacy_receiver.messages), len(_expected_results['messages']))
        for index, message in enumerate(self.legacy_receiver.messages):
            self.check_message(message, _expected_results['messages'][index])

        self.assertEquals(self.superclass_receiver.name, _expected_superclass_results['name'])
        self.assertEquals(self.superclass_receiver.superclass, _expected_superclass_results['superclass'])
        self.assertEquals(len(self.superclass_receiver.messages), len(_expected_superclass_results['messages']))
        for index, message in enumerate(self.superclass_receiver.messages):
            self.check_message(message, _expected_superclass_results['messages'][index])



class GeneratedFileContentsTest(unittest.TestCase):
    def assertGeneratedFileContentsEqual(self, first, second):
        first_list = first.split('\n')
        second_list = second.split('\n')

        for index, first_line in enumerate(first_list):
            self.assertEquals(first_line, second_list[index])

        self.assertEquals(len(first_list), len(second_list))


class HeaderTest(GeneratedFileContentsTest):
    def test_header(self):
        file_contents = messages.generate_messages_header(StringIO(_messages_file_contents))
        self.assertGeneratedFileContentsEqual(file_contents, _expected_receiver_header)

        legacy_file_contents = messages.generate_messages_header(StringIO(_legacy_messages_file_contents))
        self.assertGeneratedFileContentsEqual(legacy_file_contents, _expected_legacy_receiver_header)

        superclass_file_contents = messages.generate_messages_header(StringIO(_superclass_messages_file_contents))
        self.assertGeneratedFileContentsEqual(superclass_file_contents, _expected_superclass_receiver_header)


class ReceiverImplementationTest(GeneratedFileContentsTest):
    def test_receiver_implementation(self):
        file_contents = messages.generate_message_handler(StringIO(_messages_file_contents))
        self.assertGeneratedFileContentsEqual(file_contents, _expected_receiver_implementation)

        legacy_file_contents = messages.generate_message_handler(StringIO(_legacy_messages_file_contents))
        self.assertGeneratedFileContentsEqual(legacy_file_contents, _expected_legacy_receiver_implementation)

        superclass_file_contents = messages.generate_message_handler(StringIO(_superclass_messages_file_contents))
        self.assertGeneratedFileContentsEqual(superclass_file_contents, _expected_superclass_receiver_implementation)


class UnsupportedPrecompilerDirectiveTest(unittest.TestCase):
    def test_error_at_else(self):
        with self.assertRaisesRegexp(Exception, r"ERROR: '#else.*' is not supported in the \*\.in files"):
            messages.generate_message_handler(StringIO("asd\n#else bla\nfoo"))

    def test_error_at_elif(self):
        with self.assertRaisesRegexp(Exception, r"ERROR: '#elif.*' is not supported in the \*\.in files"):
            messages.generate_message_handler(StringIO("asd\n#elif bla\nfoo"))


if __name__ == '__main__':
    unittest.main()

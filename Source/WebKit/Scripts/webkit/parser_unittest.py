# Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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

# Examples of how to run:
# python3 Source/WebKit/Scripts/webkit/parser_unittest.py
# cd Source/WebKit/Scripts && python3 -m webkit.parser_unittest
# cd Source/WebKit/Scripts && python3 -m unittest discover -p '*_unittest.py'

import os
import re
import sys
import unittest

if sys.version_info > (3, 0):
    from io import StringIO
else:
    from StringIO import StringIO

module_directory = os.path.dirname(os.path.abspath(__file__))

sys.path.append(os.path.abspath(os.path.join(module_directory, os.path.pardir)))
from webkit import parser  # noqa: E402

tests_directory = os.path.join(module_directory, 'tests')

_expected_model_base = {
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
                ('WebCore::PlatformLayerIdentifier', 'videoLayerID'),
            ),
            'conditions': (None),
        },
        {
            'name': 'DidCreateWebProcessConnection',
            'parameters': (
                ('MachSendRight', 'connectionIdentifier'),
                ('OptionSet<WebKit::SelectionFlags>', 'flags'),
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
            'conditions': ('ENABLE(FEATURE_FOR_TESTING)'),
        }
    ),
}

_expected_model_test_without_attributes = dict(_expected_model_base, name='TestWithoutAttributes')

_expected_model_test_with_legacy_receiver = dict(_expected_model_base, name='TestWithLegacyReceiver')

_expected_model_test_with_superclass = {
    'name': 'TestWithSuperclass',
    'superclass': 'WebPageBase',
    'conditions': None,
    'messages': (
        {
            'name': 'LoadURL',
            'parameters': (
                ('String', 'url'),
            ),
            'conditions': (None),
        },
        {
            'name': 'TestAsyncMessage',
            'parameters': (
                ('WebKit::TestTwoStateEnum', 'twoStateEnum'),
            ),
            'reply_parameters': (
                ('uint64_t', 'result'),
            ),
            'conditions': ('ENABLE(TEST_FEATURE)'),
        },
        {
            'name': 'TestAsyncMessageWithNoArguments',
            'parameters': (),
            'reply_parameters': (),
            'conditions': ('ENABLE(TEST_FEATURE)'),
        },
        {
            'name': 'TestAsyncMessageWithMultipleArguments',
            'parameters': (),
            'reply_parameters': (
                ('bool', 'flag'),
                ('uint64_t', 'value'),
            ),
            'conditions': ('ENABLE(TEST_FEATURE)'),
        },
        {
            'name': 'TestAsyncMessageWithConnection',
            'parameters': (
                ('int', 'value'),
            ),
            'reply_parameters': (
                ('bool', 'flag'),
            ),
            'conditions': ('ENABLE(TEST_FEATURE)'),
        },
        {
            'name': 'TestSyncMessage',
            'parameters': (
                ('uint32_t', 'param'),
            ),
            'reply_parameters': (
                ('uint8_t', 'reply'),
            ),
            'conditions': (None),
        },
        {
            'name': 'TestSynchronousMessage',
            'parameters': (
                ('bool', 'value'),
            ),
            'reply_parameters': (
                ('std::optional<WebKit::TestClassName>', 'optionalReply'),
            ),
            'conditions': (None),
        },
    ),
}


_parsing_test_cases = [
    _expected_model_test_without_attributes,
    _expected_model_test_with_legacy_receiver,
    _expected_model_test_with_superclass
]


def parse_receiver(receiver_name):
    with open(os.path.join(tests_directory, '{}.messages.in'.format(receiver_name))) as in_file:
        return parser.parse(in_file)
    assert(False)


class ParsingTest(unittest.TestCase):
    def setUp(self):
        self.receivers = [parse_receiver(expected_model['name']) for expected_model in _parsing_test_cases]

    def check_message(self, message, expected_message):
        self.assertEqual(message.name, expected_message['name'])
        self.assertEqual(len(message.parameters), len(expected_message['parameters']))
        for index, parameter in enumerate(message.parameters):
            expected_parameter = expected_message['parameters'][index]
            self.assertEqual(parameter.type, expected_parameter[0])
            self.assertEqual(parameter.name, expected_parameter[1])
            if len(expected_parameter) > 2:
                self.assertEqual(parameter.attributes, frozenset(expected_parameter[2]))
                for attribute in expected_parameter[2]:
                    self.assertTrue(parameter.has_attribute(attribute))
            else:
                self.assertEqual(parameter.attributes, frozenset())
        if message.reply_parameters is not None:
            for index, parameter in enumerate(message.reply_parameters):
                self.assertEqual(parameter.type, expected_message['reply_parameters'][index][0])
                self.assertEqual(parameter.name, expected_message['reply_parameters'][index][1])
        else:
            self.assertFalse('reply_parameters' in expected_message)
        self.assertEqual(message.condition, expected_message['conditions'])

    def test_receiver(self):
        """Receiver should be parsed as expected"""
        for receiver, expected_model in zip(self.receivers, _parsing_test_cases):
            self.assertEqual(receiver.name, expected_model['name'])
            self.assertEqual(receiver.condition, expected_model['conditions'])
            self.assertEqual(len(receiver.messages), len(expected_model['messages']))
            for index, message in enumerate(receiver.messages):
                self.check_message(message, expected_model['messages'][index])


class UnsupportedPrecompilerDirectiveTest(unittest.TestCase):
    def test_error_at_else(self):
        with self.assertRaisesRegexp(Exception, r"ERROR: '#else.*' is not supported in the \*\.in files"):
            parser.parse(StringIO("asd\n#else bla\nfoo"))

    def test_error_at_elif(self):
        with self.assertRaisesRegexp(Exception, r"ERROR: '#elif.*' is not supported in the \*\.in files"):
            parser.parse(StringIO("asd\n#elif bla\nfoo"))

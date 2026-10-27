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

from io import StringIO

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
            'name': 'OpaqueTypeSecurityAssertion',
            'parameters': (
                ('NotDispatchableFromWebContent', 'ping'),
            ),
            'reply_parameters': (
                ('NotDispatchableFromWebContent', 'pong'),
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
            'name': 'TestAsyncMessageAnyThread',
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
        {
            'name': 'TestMessageWithMessageName',
            'parameters': (
                ('IPC::MessageName', 'messageName'),
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


def parse_swift_receiver(messages, extended_attributes=('SwiftReceiver', 'SwiftTypes'), receiver_attributes=''):
    extended_attributes = ['ExceptionForEnabledBy', 'ExceptionForDispatchedFrom', 'ExceptionForDispatchedTo'] + list(extended_attributes)
    return parser.parse(StringIO("""
[
    %s
]
messages -> TestReceiver %s{
    %s
}
""" % (',\n    '.join(extended_attributes), receiver_attributes + ' ' if receiver_attributes else '', '\n    '.join(messages))))


class SwiftTypesTest(unittest.TestCase):
    def test_receiver_and_message_attributes(self):
        receiver = parse_swift_receiver(['SwiftTyped(uint32_t value)', 'CxxTyped(uint32_t value) CxxTypes'])
        self.assertTrue(receiver.swift_types)
        self.assertTrue(receiver.takes_swift_types(receiver.messages[0]))
        self.assertFalse(receiver.takes_swift_types(receiver.messages[1]))

        receiver = parse_swift_receiver(['CxxTyped(uint32_t value)', 'SwiftTyped(uint32_t value) -> (String reply) Synchronous SwiftTypes'], extended_attributes=['SwiftReceiverBuildEnabledBy=SOME_FEATURE'])
        self.assertFalse(receiver.swift_types)
        self.assertFalse(receiver.takes_swift_types(receiver.messages[0]))
        self.assertTrue(receiver.takes_swift_types(receiver.messages[1]))

    def test_error_receiver_swift_types_without_swift_receiver(self):
        with self.assertRaisesRegex(Exception, r"ERROR: Receiver TestReceiver has 'SwiftTypes' but is not a Swift receiver"):
            parse_swift_receiver(['Message(uint32_t value)'], extended_attributes=['SwiftTypes'])

    def test_error_message_swift_types_without_swift_receiver(self):
        with self.assertRaisesRegex(Exception, r"ERROR: Message TestReceiver.Message has 'SwiftTypes' but receiver TestReceiver is not a Swift receiver"):
            parse_swift_receiver(['Message(uint32_t value) SwiftTypes'], extended_attributes=[])

    def test_error_cxx_types_without_receiver_swift_types(self):
        with self.assertRaisesRegex(Exception, r"ERROR: Message TestReceiver.Message has 'CxxTypes' but receiver TestReceiver does not have 'SwiftTypes'"):
            parse_swift_receiver(['Message(uint32_t value) CxxTypes'], extended_attributes=['SwiftReceiver'])

    def test_error_swift_types_and_cxx_types(self):
        with self.assertRaisesRegex(Exception, r"ERROR: Message TestReceiver.Message cannot have both 'SwiftTypes' and 'CxxTypes'"):
            parse_swift_receiver(['Message(uint32_t value) SwiftTypes CxxTypes'])

    def test_error_swift_types_on_stream_receiver(self):
        with self.assertRaisesRegex(Exception, r"ERROR: Receiver TestReceiver has 'SwiftTypes', which stream receivers do not support"):
            parse_swift_receiver(['Message(uint32_t value)'], receiver_attributes='Stream')
        with self.assertRaisesRegex(Exception, r"ERROR: Message TestReceiver.Message takes Swift types, which is not supported on a stream receiver"):
            parse_swift_receiver(['Message(uint32_t value) SwiftTypes'], extended_attributes=['SwiftReceiver'], receiver_attributes='Stream')

    def test_error_unmappable_parameter(self):
        with self.assertRaisesRegex(Exception, r"ERROR: Message TestReceiver.Message takes Swift types, but cannot take parameter 'url': 'URL' has no Swift form"):
            parse_swift_receiver(['Message(uint32_t value, URL url)'])
        with self.assertRaisesRegex(Exception, r"ERROR: Message TestReceiver.Message takes Swift types, but cannot take parameter 'values': 'HashMap<String, uint32_t>' has no Swift form"):
            parse_swift_receiver(['Message(HashMap<String, uint32_t> values)'])

    def test_error_unmappable_reply(self):
        with self.assertRaisesRegex(Exception, r"ERROR: Message TestReceiver.Message takes Swift types, but cannot take reply parameter 'result': 'std::expected<uint32_t, WebKit::Error>' has no Swift form"):
            parse_swift_receiver(['Message(uint32_t value) -> (std::expected<uint32_t, WebKit::Error> result)'])

    def test_error_call_with_reply_id(self):
        with self.assertRaisesRegex(Exception, r"ERROR: Message TestReceiver.Message takes Swift types, which is not supported together with 'CallWithReplyID'"):
            parse_swift_receiver(['Message(uint32_t value) -> () CallWithReplyID'])

    def test_error_unsupported_receiver_attributes(self):
        for attribute in ('NotUsingIPCConnection', 'WantsDispatchMessage', 'WantsAsyncDispatchMessage'):
            with self.assertRaisesRegex(Exception, r"ERROR: Message TestReceiver.Message takes Swift types, which is not supported on a receiver with '%s'" % attribute):
                parse_swift_receiver(['Message(uint32_t value)'], receiver_attributes=attribute)

    def test_error_validator(self):
        with self.assertRaisesRegex(Exception, r"ERROR: Message TestReceiver.Message takes Swift types, which is not supported together with 'Validator=isAllowed'"):
            parse_swift_receiver(['[Validator=isAllowed] Message(uint32_t value)'])

    def test_error_synchronous_without_reply(self):
        with self.assertRaisesRegex(Exception, r"ERROR: Message TestReceiver.Message takes Swift types, which is not supported for a 'Synchronous' message without a reply"):
            parse_swift_receiver(['Message(uint32_t value) Synchronous'])

    def test_error_untrusted_parameter(self):
        with self.assertRaisesRegex(Exception, r"ERROR: Message TestReceiver.Message takes Swift types, which is not supported with an IPC::Untrusted<> parameter 'value'"):
            parse_swift_receiver(['Message(IPC::Untrusted<uint32_t> value)'])

    def test_error_parameter_attributes(self):
        with self.assertRaisesRegex(Exception, r"ERROR: Message TestReceiver.Message takes Swift types, which is not supported with attributes on parameter 'value': AttributeOne"):
            parse_swift_receiver(['Message([AttributeOne] uint32_t value)'])

    def test_error_reserved_parameter_name(self):
        for name in ('connection', 'decoder', 'replyEncoder', 'reply', 'target'):
            with self.assertRaisesRegex(Exception, r"ERROR: Message TestReceiver.Message takes Swift types, so cannot have a parameter named '%s'" % name):
                parse_swift_receiver(['Message(uint32_t %s) -> (uint32_t reply)' % name])

    def test_error_ref_reply(self):
        for reply_type in ('Ref<WebKit::Node>', 'WTF::Ref<WebKit::Node>', 'const Ref<WebKit::Node>&'):
            with self.assertRaisesRegex(Exception, r"ERROR: Message TestReceiver.Message takes Swift types, so its reply parameter 'node' cannot be a Ref<>.*use RefPtr<> or std::optional<> instead"):
                parse_swift_receiver(['Message(uint32_t value) -> (uint32_t count, %s node)' % reply_type])
        receiver = parse_swift_receiver(['Message(Ref<WebKit::Node> node) -> (RefPtr<WebKit::Node> parent, Vector<Ref<WebKit::Node>> children, std::optional<Ref<WebKit::Node>> sibling)'])
        self.assertTrue(receiver.takes_swift_types(receiver.messages[0]))
        receiver = parse_swift_receiver(['Message(uint32_t value) -> (Ref<WebKit::Node> node) CxxTypes'])
        self.assertFalse(receiver.takes_swift_types(receiver.messages[0]))

    def test_cxx_typed_messages_are_not_checked(self):
        receiver = parse_swift_receiver(['[Validator=isAllowed] Message(URL url, uint32_t reply) -> () CallWithReplyID CxxTypes'])
        self.assertFalse(receiver.takes_swift_types(receiver.messages[0]))


class UnsupportedPrecompilerDirectiveTest(unittest.TestCase):
    def test_error_at_else(self):
        with self.assertRaisesRegex(Exception, r"ERROR: '#else.*' is not supported in the \*\.in files"):
            parser.parse(StringIO("asd\n#else bla\nfoo"))

    def test_error_at_elif(self):
        with self.assertRaisesRegex(Exception, r"ERROR: '#elif.*' is not supported in the \*\.in files"):
            parser.parse(StringIO("asd\n#elif bla\nfoo"))


if __name__ == '__main__':
    unittest.main()

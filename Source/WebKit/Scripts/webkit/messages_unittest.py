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

import os
import re
import sys
import unittest
from StringIO import StringIO

sys.path.append(os.path.dirname(os.path.dirname(__file__)))
from webkit import messages
from webkit import parser

script_directory = os.path.dirname(os.path.realpath(__file__))

reset_results = False

with open(os.path.join(script_directory, 'test-messages.in')) as in_file:
    _messages_file_contents = in_file.read()

with open(os.path.join(script_directory, 'test-legacy-messages.in')) as in_file:
    _legacy_messages_file_contents = in_file.read()

with open(os.path.join(script_directory, 'test-superclass-messages.in')) as in_file:
    _superclass_messages_file_contents = in_file.read()

_expected_receiver_header_file_name = 'Messages-expected.h'
_expected_legacy_receiver_header_file_name = 'LegacyMessages-expected.h'
_expected_superclass_receiver_header_file_name = 'MessagesSuperclass-expected.h'

_expected_receiver_implementation_file_name = 'MessageReceiver-expected.cpp'
_expected_legacy_receiver_implementation_file_name = 'LegacyMessageReceiver-expected.cpp'
_expected_superclass_receiver_implementation_file_name = 'MessageReceiverSuperclass-expected.cpp'

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
            'name': 'TestDelayedMessage',
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
        if message.reply_parameters is not None:
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
    def assertGeneratedFileContentsEqual(self, actual_file_contents, expected_file_name):
        try:
            if reset_results:
                with open(os.path.join(script_directory, expected_file_name), mode='w') as out_file:
                    out_file.write(actual_file_contents)
                return

            with open(os.path.join(script_directory, expected_file_name), mode='r') as in_file:
                expected_file_contents = in_file.read()
            actual_line_list = actual_file_contents.splitlines(False)
            expected_line_list = expected_file_contents.splitlines(False)

            for index, actual_line in enumerate(actual_line_list):
                self.assertEquals(actual_line, expected_line_list[index])

            self.assertEquals(len(actual_line_list), len(expected_line_list))
        except:
            sys.stderr.write('In expected file %s\n' % expected_file_name)
            raise

    def assertHeaderEqual(self, input_messages_file_contents, expected_file_name):
        actual_file_contents = messages.generate_messages_header(StringIO(input_messages_file_contents))
        self.assertGeneratedFileContentsEqual(actual_file_contents, expected_file_name)

    def assertImplementationEqual(self, input_messages_file_contents, expected_file_name):
        actual_file_contents = messages.generate_message_handler(StringIO(input_messages_file_contents))
        self.assertGeneratedFileContentsEqual(actual_file_contents, expected_file_name)


class HeaderTest(GeneratedFileContentsTest):
    def test_receiver_headers(self):
        self.assertHeaderEqual(_messages_file_contents,
                               _expected_receiver_header_file_name)
        self.assertHeaderEqual(_legacy_messages_file_contents,
                               _expected_legacy_receiver_header_file_name)
        self.assertHeaderEqual(_superclass_messages_file_contents,
                               _expected_superclass_receiver_header_file_name)


class ReceiverImplementationTest(GeneratedFileContentsTest):
    def test_receiver_implementations(self):
        self.assertImplementationEqual(_messages_file_contents,
                                       _expected_receiver_implementation_file_name)
        self.assertImplementationEqual(_legacy_messages_file_contents,
                                       _expected_legacy_receiver_implementation_file_name)
        self.assertImplementationEqual(_superclass_messages_file_contents,
                                       _expected_superclass_receiver_implementation_file_name)


class UnsupportedPrecompilerDirectiveTest(unittest.TestCase):
    def test_error_at_else(self):
        with self.assertRaisesRegexp(Exception, r"ERROR: '#else.*' is not supported in the \*\.in files"):
            messages.generate_message_handler(StringIO("asd\n#else bla\nfoo"))

    def test_error_at_elif(self):
        with self.assertRaisesRegexp(Exception, r"ERROR: '#elif.*' is not supported in the \*\.in files"):
            messages.generate_message_handler(StringIO("asd\n#elif bla\nfoo"))


def add_reset_results_to_unittest_help():
    script_name = os.path.basename(__file__)
    reset_results_help = '''
Custom Options:
  -r, --reset-results  Reset expected results for {0}
'''.format(script_name)

    options_regex = re.compile('^Usage:')
    lines = unittest.TestProgram.USAGE.splitlines(True)
    index = 0
    for index, line in enumerate(lines):
        if options_regex.match(line) and index + 1 < len(lines):
            lines.insert(index + 1, reset_results_help)
            break

    if index == (len(lines) - 1):
        lines.append(reset_results_help)

    unittest.TestProgram.USAGE = ''.join(lines)


def parse_sys_argv():
    global reset_results
    for index, arg in enumerate(sys.argv[1:]):
        if arg in ('-r', '--r', '--reset', '--reset-results') or '--reset-results'.startswith(arg):
            reset_results = True
            del sys.argv[index + 1]
            break


if __name__ == '__main__':
    add_reset_results_to_unittest_help()
    parse_sys_argv()
    unittest.main()

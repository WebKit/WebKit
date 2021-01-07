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
# python Source/WebKit/Scripts/webkit/messages_unittest.py [-r]
# cd Source/WebKit/Scripts && python -m webkit.messages_unittest
# cd Source/WebKit/Scripts && python -m unittest discover -p '*_unittest.py'

import os
import re
import sys
import unittest

module_directory = os.path.dirname(os.path.abspath(__file__))

sys.path.append(os.path.abspath(os.path.join(module_directory, os.path.pardir)))
from webkit import messages, model, parser  # noqa: E402

tests_directory = os.path.join(module_directory, 'tests')

reset_results = False

_test_receiver_names = ['TestWithSuperclass', 'TestWithLegacyReceiver', 'TestWithoutAttributes', 'TestWithIfMessage']


def receiver_header_file_name(receiver_name):
    return '%sMessages.h' % receiver_name


def receiver_implementation_file_name(receiver_name):
    return '%sMessageReceiver.cpp' % receiver_name


def parse_receiver(receiver_name):
    with open(os.path.join(tests_directory, '{}.messages.in'.format(receiver_name))) as in_file:
        return parser.parse(in_file)
    assert(False)


class GeneratedFileContentsTest(unittest.TestCase):
    def setUp(self):
        self.test_receivers = [parse_receiver(receiver_name) for receiver_name in _test_receiver_names]
        self.receivers = model.generate_global_model(self.test_receivers)

    def assertGeneratedFileContentsEqual(self, actual_file_contents, expected_file_name):
        try:
            if reset_results:
                with open(os.path.join(tests_directory, expected_file_name), mode='w') as out_file:
                    out_file.write(actual_file_contents)
                return

            with open(os.path.join(tests_directory, expected_file_name), mode='r') as in_file:
                expected_file_contents = in_file.read()
            actual_line_list = actual_file_contents.splitlines(False)
            expected_line_list = expected_file_contents.splitlines(False)

            for index, actual_line in enumerate(actual_line_list):
                self.assertEquals(actual_line, expected_line_list[index])

            self.assertEquals(len(actual_line_list), len(expected_line_list))
        except Exception:
            sys.stderr.write('In expected file %s\n' % expected_file_name)
            raise

    def test_receiver(self):
        for receiver_name, receiver in zip(_test_receiver_names, self.test_receivers):
            header_contents = messages.generate_messages_header(receiver)
            self.assertGeneratedFileContentsEqual(header_contents, os.path.join(tests_directory, '{}Messages.h'.format(receiver_name)))
            implementation_contents = messages.generate_message_handler(receiver)
            self.assertGeneratedFileContentsEqual(implementation_contents, os.path.join(tests_directory, '{}MessageReceiver.cpp'.format(receiver_name)))

    def test_message_names(self):
        header_contents = messages.generate_message_names_header(self.receivers)
        self.assertGeneratedFileContentsEqual(header_contents, os.path.join(tests_directory, 'MessageNames.h'))
        implementation_contents = messages.generate_message_names_implementation(self.receivers)
        self.assertGeneratedFileContentsEqual(implementation_contents, os.path.join(tests_directory, 'MessageNames.cpp'))

    def test_message_argument_description(self):
        receiver_header_files = ['{}Messages.h'.format(receiver.name) for receiver in self.receivers]
        implementation_contents = messages.generate_message_argument_description_implementation(self.receivers, receiver_header_files)
        self.assertGeneratedFileContentsEqual(implementation_contents, os.path.join(tests_directory, 'MessageArgumentDescriptions.cpp'))

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

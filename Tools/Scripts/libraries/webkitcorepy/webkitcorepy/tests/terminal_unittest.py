# Copyright (C) 2021, 2022 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import io
import logging
import sys
import typing
import unittest

from mock import patch

from webkitcorepy import OutputCapture, Terminal, mocks


class TerminalTests(unittest.TestCase):
    def test_choose_basic(self):
        with mocks.Terminal.input('y'), OutputCapture() as captured:
            self.assertEqual('Yes', Terminal.choose('Continue'))
        self.assertEqual(captured.stdout.getvalue(), 'Continue (Yes/No): \n')

        with mocks.Terminal.input('n'), OutputCapture() as captured:
            self.assertEqual('No', Terminal.choose('Continue'))
        self.assertEqual(captured.stdout.getvalue(), 'Continue (Yes/No): \n')

        with mocks.Terminal.input('huh', 'y'), OutputCapture() as captured:
            self.assertEqual('Yes', Terminal.choose('Continue'))
        self.assertEqual(captured.stdout.getvalue(), "Continue (Yes/No): \n'huh' is not an option\nContinue (Yes/No): \n")

    def test_choose_strict(self):
        with mocks.Terminal.input('Yes'), OutputCapture() as captured:
            self.assertEqual('Yes', Terminal.choose('Continue', strict=True))
        self.assertEqual(captured.stdout.getvalue(), 'Continue (Yes/No): \n')

        with mocks.Terminal.input('No'), OutputCapture() as captured:
            self.assertEqual('No', Terminal.choose('Continue', strict=True))
        self.assertEqual(captured.stdout.getvalue(), 'Continue (Yes/No): \n')

        with mocks.Terminal.input('n', 'Yes'), OutputCapture() as captured:
            self.assertEqual('Yes', Terminal.choose('Continue', strict=True))
        self.assertEqual(captured.stdout.getvalue(), "Continue (Yes/No): \n'n' is not an option\nContinue (Yes/No): \n")

        with mocks.Terminal.input('y', 'No'), OutputCapture() as captured:
            self.assertEqual('No', Terminal.choose('Continue', strict=True))
        self.assertEqual(captured.stdout.getvalue(), "Continue (Yes/No): \n'y' is not an option\nContinue (Yes/No): \n")

    def test_choose_default(self):
        with mocks.Terminal.input('y'), OutputCapture() as captured:
            self.assertEqual('Yes', Terminal.choose('Continue', default='Other'))
        self.assertEqual(captured.stdout.getvalue(), 'Continue (Yes/No): \n')

        with mocks.Terminal.input('n'), OutputCapture() as captured:
            self.assertEqual('No', Terminal.choose('Continue', default='Other'))
        self.assertEqual(captured.stdout.getvalue(), 'Continue (Yes/No): \n')

        with mocks.Terminal.input('huh'), OutputCapture() as captured:
            self.assertEqual('Other', Terminal.choose('Continue', default='Other'))
        self.assertEqual(captured.stdout.getvalue(), 'Continue (Yes/No): \n')

    def test_choose_triple(self):
        with mocks.Terminal.input('y'), OutputCapture() as captured:
            self.assertEqual('Yes', Terminal.choose('Continue', options=('Yes', 'No', 'Maybe')))
        self.assertEqual(captured.stdout.getvalue(), 'Continue (Yes/No/Maybe): \n')

        with mocks.Terminal.input('n'), OutputCapture() as captured:
            self.assertEqual('No', Terminal.choose('Continue', options=('Yes', 'No', 'Maybe')))
        self.assertEqual(captured.stdout.getvalue(), 'Continue (Yes/No/Maybe): \n')

        with mocks.Terminal.input('may'), OutputCapture() as captured:
            self.assertEqual('Maybe', Terminal.choose('Continue', options=('Yes', 'No', 'Maybe')))
        self.assertEqual(captured.stdout.getvalue(), 'Continue (Yes/No/Maybe): \n')

        with mocks.Terminal.input('huh'), OutputCapture() as captured:
            self.assertEqual('No', Terminal.choose('Continue', options=('Yes', 'No', 'Maybe'), default='No'))
        self.assertEqual(captured.stdout.getvalue(), 'Continue (Yes/[No]/Maybe): \n')

    def test_choose_number(self):
        with mocks.Terminal.input('2'), OutputCapture() as captured:
            self.assertEqual('Beta', Terminal.choose('Pick', options=('Alpha', 'Beta', 'Charlie', 'Delta'), numbered=True))
        self.assertEqual(captured.stdout.getvalue(), 'Pick:\n    1) Alpha\n    2) Beta\n    3) Charlie\n    4) Delta\n: \n')

    def test_interrupt(self):
        def do_interrupt(output):
            print(output)
            raise KeyboardInterrupt

        if sys.version_info > (3, 0):
            mocked = patch('builtins.input', new=do_interrupt)
        else:
            import __builtin__
            mocked = patch.object(__builtin__, 'raw_input', new=do_interrupt)

        with OutputCapture() as captured, self.assertRaises(SystemExit) as caught, mocked:
            Terminal.choose('Continue')

        self.assertEqual(caught.exception.code, 1)
        self.assertEqual(captured.stderr.getvalue(), '\nUser interrupted program\n')

    def test_interrupt_decorator(self):
        with OutputCapture() as captured, self.assertRaises(SystemExit) as caught:
            with Terminal.disable_keyboard_interrupt_stacktracktrace(logging.root.level - 1):
                raise KeyboardInterrupt
        self.assertEqual(caught.exception.code, 1)
        self.assertEqual(captured.stderr.getvalue(), '\nUser interrupted program\n')

        with self.assertRaises(KeyboardInterrupt):
            with Terminal.disable_keyboard_interrupt_stacktracktrace(logging.root.level):
                raise KeyboardInterrupt

    def test_assert_writeable_stream(self):
        for file_like in (
            io.BytesIO(),
            io.StringIO(),
            sys.stdout,
            sys.stderr,
        ):
            Terminal.assert_writeable_stream(file_like)

        for file_like in (
            sys.stdin,
            typing.IO(),
        ):
            with self.assertRaises(ValueError):
                Terminal.assert_writeable_stream(file_like)

    def test_override_atty(self):
        for file_like in (
            io.BytesIO(),
            io.StringIO(),
            sys.stdin,
            sys.stdout,
            sys.stderr,
            typing.IO(),
        ):
            original = Terminal.isatty(file_like)

            with Terminal.override_atty(file_like, isatty=False):
                self.assertFalse(Terminal.isatty(file_like))

            self.assertEqual(original, Terminal.isatty(file_like))

            with Terminal.override_atty(file_like, isatty=True):
                self.assertTrue(Terminal.isatty(file_like))

            self.assertEqual(original, Terminal.isatty(file_like))

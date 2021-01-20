# Copyright (C) 2020 Apple Inc. All rights reserved.
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

import mock
import unittest

from webkitcorepy import mocks


def to_be_replaced():
    return None


class ExampleStack(mocks.ContextStack):
    top = None

    def __init__(self):
        super(ExampleStack, self).__init__(cls=ExampleStack)
        self.patches.append(mock.patch(
            'webkitcorepy.tests.mocks.context_stack_unittest.to_be_replaced',
            new=lambda: str(self),
        ))

    @classmethod
    def height(cls):
        count = 0
        current = cls.top
        while current:
            current = current.previous
            count += 1
        return count


class ContextStack(unittest.TestCase):

    def test_stacking(self):
        stack_1 = ExampleStack()
        stack_2 = ExampleStack()

        self.assertIsNone(ExampleStack.top)
        self.assertEqual(0, ExampleStack.height())
        with stack_1:
            self.assertEqual(1, ExampleStack.height())
            self.assertEqual(ExampleStack.top, stack_1)
            self.assertIsNone(ExampleStack.top.previous)

            with stack_2:
                self.assertEqual(2, ExampleStack.height())
                self.assertEqual(ExampleStack.top, stack_2)
                self.assertEqual(ExampleStack.top.previous, stack_1)

            self.assertEqual(1, ExampleStack.height())
            self.assertEqual(ExampleStack.top, stack_1)
            self.assertIsNone(ExampleStack.top.previous)

    def test_patch(self):
        stack_1 = ExampleStack()
        stack_2 = ExampleStack()

        self.assertIsNone(to_be_replaced())
        with stack_1:
            self.assertEqual(str(stack_1), to_be_replaced())
            with stack_2:
                self.assertEqual(str(stack_2), to_be_replaced())

            self.assertEqual(str(stack_1), to_be_replaced())

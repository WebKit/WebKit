# Copyright (C) 2026 Apple Inc. All rights reserved.
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
from unittest.mock import MagicMock

from webkitpy.style.checkers.swift_association import SwiftAssociationChecker


CWD = '/mock-checkout'
CPP = 'Source/WebKit/UIProcess/WebBackForwardList.cpp'
SWIFT = 'Source/WebKit/UIProcess/WebBackForwardList.swift'


def _abs(rel_path):
    return os.path.join(CWD, rel_path)


class SwiftAssociationCheckerTest(unittest.TestCase):
    def _check(self, files):
        configuration = MagicMock()
        configuration.is_reportable.return_value = True
        increment = MagicMock()
        SwiftAssociationChecker.check_associations(files, configuration, CWD, increment)
        return configuration, increment

    def test_cpp_modified_without_swift_is_flagged(self):
        configuration, increment = self._check({_abs(CPP): [1, 2, 3]})

        self.assertEqual(configuration.write_style_error.call_count, 1)
        self.assertEqual(increment.call_count, 1)
        call = configuration.write_style_error.call_args
        self.assertEqual(call.kwargs['category'], 'swift/association')
        self.assertEqual(call.kwargs['line_number'], 0)
        self.assertEqual(call.kwargs['file_path'], _abs(CPP))
        self.assertIn(CPP, call.kwargs['message'])
        self.assertIn(SWIFT, call.kwargs['message'])

    def test_swift_modified_without_cpp_is_flagged(self):
        configuration, increment = self._check({_abs(SWIFT): [4, 5]})

        self.assertEqual(configuration.write_style_error.call_count, 1)
        call = configuration.write_style_error.call_args
        self.assertEqual(call.kwargs['file_path'], _abs(SWIFT))
        self.assertIn(SWIFT, call.kwargs['message'])
        self.assertIn(CPP, call.kwargs['message'])

    def test_both_modified_is_not_flagged(self):
        configuration, increment = self._check({_abs(CPP): [1], _abs(SWIFT): [2]})

        configuration.write_style_error.assert_not_called()
        increment.assert_not_called()

    def test_removed_cpp_without_swift_is_flagged(self):
        # Removed files map to None; the counterpart is still required to change.
        configuration, increment = self._check({_abs(CPP): None})

        self.assertEqual(configuration.write_style_error.call_count, 1)

    def test_unrelated_file_is_not_flagged(self):
        configuration, increment = self._check({_abs('Source/WebKit/UIProcess/WebPageProxy.cpp'): [1]})

        configuration.write_style_error.assert_not_called()
        increment.assert_not_called()

    def test_filtered_out_errors_are_not_counted(self):
        configuration = MagicMock()
        configuration.is_reportable.return_value = False
        increment = MagicMock()

        SwiftAssociationChecker.check_associations({_abs(CPP): [1]}, configuration, CWD, increment)

        configuration.write_style_error.assert_not_called()
        increment.assert_not_called()


if __name__ == '__main__':
    unittest.main()

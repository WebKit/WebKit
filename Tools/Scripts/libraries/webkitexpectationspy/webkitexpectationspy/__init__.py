# Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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

"""webkitexpectationspy - Test expectations management for WebKit.

A system for parsing, querying, and linting test expectation files.
Supports API tests and layout tests with suite-specific plugins.

Example usage:
    from webkitexpectationspy import ExpectationsManager, ResultStatus
    from webkitexpectationspy.suites import APITestSuite

    manager = ExpectationsManager(suite=APITestSuite())
    manager.load_file('TestExpectations')

    exp = manager.get_expectation('TestWebKitAPI.WTF.StringTest')
    if exp.skip:
        print('Test is skipped')
    if ResultStatus.FAIL in exp.expected:
        print('Test expected to fail')
"""

__version__ = '3.0.0'

from webkitexpectationspy.expectations import Expectation, ResultStatus
from webkitexpectationspy.modifiers import ModifiersBase, LayoutTestModifiers
from webkitexpectationspy.suites.layout_tests import LayoutTestStatus
from webkitexpectationspy.manager import ExpectationsManager

__all__ = [
    '__version__',
    'Expectation',
    'ExpectationsManager',
    'ResultStatus',
    'LayoutTestStatus',
    'ModifiersBase',
    'LayoutTestModifiers',
]

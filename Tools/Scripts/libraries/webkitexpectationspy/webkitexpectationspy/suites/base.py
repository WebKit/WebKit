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

"""Generic, instantiable base class for test suite formats.

``TestSuiteFormat()`` is directly usable for any suite whose results fit
``pass``/``fail``/``crash``/``timeout`` with ``skip``/``slow`` modifiers.
Suites with richer status sets (layout tests) or extra modifiers subclass
and override only the differing pieces.
"""

from typing import Dict, Set, Optional, List, Type, Tuple

from webkitexpectationspy.configuration import ConfigurationCategory
from webkitexpectationspy.expectations import ResultStatus
from webkitexpectationspy.modifiers import ModifiersBase


class TestSuiteFormat:

    @property
    def name(self) -> str:
        return 'generic'

    @property
    def expectation_map(self) -> Dict:
        return {
            'pass': ResultStatus.PASS,
            'fail': ResultStatus.FAIL,
            'failure': ResultStatus.FAIL,
            'crash': ResultStatus.CRASH,
            'timeout': ResultStatus.TIMEOUT,
        }

    @property
    def modifier_map(self) -> Set[str]:
        return {'skip', 'slow'}

    @property
    def modifier_class(self) -> Type[ModifiersBase]:
        return ModifiersBase

    @property
    def status_type(self) -> type:
        return type(next(iter(self.expectation_map.values())))

    @property
    def status_names(self) -> Dict:
        return {member: str(member) for member in self.status_type}

    @property
    def expectation_order(self) -> Tuple:
        return tuple(self.status_type)

    @property
    def version_tokens(self) -> Set[str]:
        return set()

    @property
    def version_order(self) -> List[str]:
        return []

    @property
    def additional_tokens(self) -> Dict[ConfigurationCategory, Set[str]]:
        return {}

    def matches_test(self, pattern: str, test_name: str) -> bool:
        return pattern == test_name

    def is_wildcard_pattern(self, pattern: str) -> bool:
        return False

    def validate_test_pattern(self, pattern: str) -> Optional[str]:
        if pattern and not pattern.isspace():
            return pattern
        return None

    def supports_custom_timeout(self) -> bool:
        return True

    def result_was_expected(self, actual_status, expected, modifiers=None) -> bool:
        if actual_status in expected:
            return True
        # SKIP is both a result and a modifier; a skipped test that reports SKIP
        # is still considered to match the expectation.
        if modifiers and modifiers.skip:
            if hasattr(actual_status, 'name') and actual_status.name == 'SKIP':
                return True
        return False

    def get_expectation_name(self, constant) -> str:
        return str(constant)

# Copyright (C) 2026 Apple Inc. All rights reserved.
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

"""Which EWS builders this dashboard reads, and how each one reports its failures.

Layout-tests and API-tests share the retry shape in Tools/CISupport/ews-build/steps.py — run,
rerun with the change, run again on a clean tree, diff — but not the way they publish the three
failure lists:

  layout-tests  RunWebKitTests and friends set first_run_failures, second_run_failures and
                clean_tree_run_failures as build properties, readable straight off the build.
  api-tests     RunAPITests.parse_and_set_failures sets those three properties too, but ingest
                reads the retry steps' JSON logs instead, unioning each log's Timedout, Crashed and
                Failed entries the way that step's own parser does.

Since WebKit commits f554adccfd9e (layout-tests) and 60edc560c7b8 (api-tests) both suites also
publish first_run_failures_filtered and second_run_failures_filtered, with pre-existing flaky
failures already removed from author blame. Those are what EWS actually showed the author, so
ingest prefers them over the raw lists whenever a build has both.

The builder patterns are checked against the fixtures in ews-build's factories_unittest.py rather
than guessed, which is what the exclusions below are for: JSC-Tests-x86-64-EWS, Bindings-Tests-EWS,
WebKitPy-Tests-EWS and WebKitPerl-Tests-EWS all end in -Tests-EWS but run through factories that
never touch RunWebKitTests, and macOS-Tahoe-Debug-API-Tests-EWS ends in -Tests-EWS while being an
API-tests builder.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from typing import Optional


@dataclass(frozen=True)
class Suite:
    name: str
    builder_pattern: 're.Pattern[str]'
    first_run_property: str
    second_run_property: str
    clean_tree_property: str
    retry_step_names: tuple[str, ...] = ()
    failures_from_logs: bool = False

    def owns(self, builder_name: str) -> bool:
        return bool(self.builder_pattern.search(builder_name))


# API-tests is checked first, so an embedded-form builder like macOS-Tahoe-Debug-API-Tests-EWS
# resolves to api-tests rather than matching the layout-tests suffix.
SUITES = (
    Suite(
        name='api-tests',
        builder_pattern=re.compile(r'API-Tests-'),
        first_run_property='first_run_failures',
        second_run_property='second_run_failures',
        clean_tree_property='clean_tree_run_failures',
        retry_step_names=('run-api-tests', 're-run-api-tests', 'run-api-tests-without-change'),
        failures_from_logs=True,
    ),
    Suite(
        name='layout-tests',
        builder_pattern=re.compile(
            r'^(?!.*API-Tests-)(?!JSC-)(?!Bindings-Tests-EWS$)'
            r'(?!WebKitPy-Tests-EWS$)(?!WebKitPerl-Tests-EWS$)'
            r'.*(-Tests-EWS$|-WPT-.*-Tests-EWS$)'
        ),
        first_run_property='first_run_failures',
        second_run_property='second_run_failures',
        clean_tree_property='clean_tree_run_failures',
        retry_step_names=('layout-tests', 're-run-layout-tests', 'run-layout-tests-without-change'),
    ),
)


def suite_for_builder(builder_name: str) -> Optional[Suite]:
    for suite in SUITES:
        if suite.owns(builder_name):
            return suite
    return None

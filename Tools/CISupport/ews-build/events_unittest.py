# Copyright (C) 2020-2023 Apple Inc. All rights reserved.
# Copyright (C) 2021 Igalia S.L.
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

import time
import unittest

from .events import CommitClassifier, GitHubEventHandlerNoEdits, MAX_FAILING_TESTS_TO_REPORT, failing_tests_for_build


class TestFailingTestsForBuild(unittest.TestCase):
    def test_none_or_empty_properties(self) -> None:
        self.assertEqual(failing_tests_for_build(None), ([], 0, ''))
        self.assertEqual(failing_tests_for_build({}), ([], 0, ''))

    def test_layout_test_failures_are_sorted(self) -> None:
        properties = {
            'new_failures_introduced_by_patch': [
                [
                    'imported/w3c/web-platform-tests/css/css-flexbox/flex-wrap-balance-002.html',
                    'imported/w3c/web-platform-tests/css/css-flexbox/flex-wrap-balance-001.html',
                ],
                'Layout Tests',
            ],
        }
        self.assertEqual(failing_tests_for_build(properties), ([
            'imported/w3c/web-platform-tests/css/css-flexbox/flex-wrap-balance-001.html',
            'imported/w3c/web-platform-tests/css/css-flexbox/flex-wrap-balance-002.html',
        ], 2, 'layout'))

    def test_layout_group_wins_over_api_group(self) -> None:
        properties = {
            'new_failures_introduced_by_patch': [
                ['imported/w3c/web-platform-tests/css/css-flexbox/flex-wrap-balance-001.html'],
                'Layout Tests',
            ],
            'new_api_failures_introduced_by_patch': [
                ['TestWebKitAPI.ContextMenuTests.MenuTrackingCancelledWhenPageCloses'],
                'API Tests',
            ],
        }
        names, count, category = failing_tests_for_build(properties)
        self.assertEqual(names, ['imported/w3c/web-platform-tests/css/css-flexbox/flex-wrap-balance-001.html'])
        self.assertEqual(count, 1)
        self.assertEqual(category, 'layout')
        self.assertNotIn('TestWebKitAPI.ContextMenuTests.MenuTrackingCancelledWhenPageCloses', names)

    def test_api_group_reports_api_category(self) -> None:
        properties = {
            'new_api_failures_introduced_by_patch': [
                ['TestWebKitAPI.ContextMenuTests.MenuTrackingCancelledWhenPageCloses'],
                'API Tests',
            ],
        }
        names, count, category = failing_tests_for_build(properties)
        self.assertEqual(names, ['TestWebKitAPI.ContextMenuTests.MenuTrackingCancelledWhenPageCloses'])
        self.assertEqual(count, 1)
        self.assertEqual(category, 'api')

    def test_static_analysis_group_reports_static_analysis_category(self) -> None:
        properties = {
            'test_failures': [
                ['some/safer-cpp-violation.cpp'],
                'Safer C++',
            ],
        }
        names, count, category = failing_tests_for_build(properties)
        self.assertEqual(names, ['some/safer-cpp-violation.cpp'])
        self.assertEqual(count, 1)
        self.assertEqual(category, 'static-analysis')

    def test_jsc_properties_are_unioned_and_deduplicated(self) -> None:
        properties = {
            'jsc_stress_test_failures_filtered': [
                ['stress/regress-flex-wrap-balance.js', 'stress/regress-shared.js'],
                'JSC Tests',
            ],
            'jsc_binary_failures_filtered': [
                ['testapi.regress-shared', 'stress/regress-shared.js'],
                'JSC Tests',
            ],
        }
        names, count, category = failing_tests_for_build(properties)
        self.assertEqual(names, ['stress/regress-flex-wrap-balance.js', 'stress/regress-shared.js', 'testapi.regress-shared'])
        self.assertEqual(count, 3)
        self.assertEqual(category, 'jsc')

    def test_over_cap_input_is_capped_but_count_is_true_total(self) -> None:
        test_names = [f'imported/w3c/web-platform-tests/css/css-flexbox/flex-wrap-balance-{i:03}.html' for i in range(MAX_FAILING_TESTS_TO_REPORT + 5)]
        properties = {
            'test_failures': [test_names, 'Layout Tests'],
        }
        names, count, category = failing_tests_for_build(properties)
        self.assertEqual(len(names), MAX_FAILING_TESTS_TO_REPORT)
        self.assertEqual(count, MAX_FAILING_TESTS_TO_REPORT + 5)
        self.assertEqual(names, sorted(test_names)[:MAX_FAILING_TESTS_TO_REPORT])
        self.assertEqual(category, 'static-analysis')

    def test_malformed_values_are_skipped_without_raising(self) -> None:
        properties = {
            'new_failures_introduced_by_patch': [
                'imported/w3c/web-platform-tests/css/css-flexbox/flex-wrap-balance-001.html',
                'Layout Tests',
            ],
            'new_api_failures_introduced_by_patch': [
                ['TestWebKitAPI.ContextMenuTests.MenuTrackingCancelledWhenPageCloses', 12345],
                'API Tests',
            ],
        }
        names, count, category = failing_tests_for_build(properties)
        self.assertEqual(names, ['TestWebKitAPI.ContextMenuTests.MenuTrackingCancelledWhenPageCloses'])
        self.assertEqual(count, 1)
        self.assertEqual(category, 'api')

    def test_none_property_entry_does_not_raise(self) -> None:
        properties = {
            'new_failures_introduced_by_patch': None,
        }
        self.assertEqual(failing_tests_for_build(properties), ([], 0, ''))

    def test_empty_list_property_entry_does_not_raise(self) -> None:
        properties = {
            'new_failures_introduced_by_patch': [],
        }
        self.assertEqual(failing_tests_for_build(properties), ([], 0, ''))


class TestCommitClassifier(unittest.TestCase):
    def test_regex_header_filter(self):
        self.assertTrue(CommitClassifier.LineFilter('^[Vv]ersioning\\.?$')('Versioning.'))
        self.assertTrue(CommitClassifier.LineFilter('^[Vv]ersioning\\.?$')('versioning.'))
        self.assertTrue(CommitClassifier.LineFilter('^[Vv]ersioning\\.?$')('Versioning'))

        self.assertFalse(CommitClassifier.LineFilter('^[Vv]ersioning\\.?$')('Bumped versioning'))
        self.assertFalse(CommitClassifier.LineFilter('^[Vv]ersioning\\.?$')('Versioning bumped.'))

    def test_fuzzy_header_filter(self):
        self.assertTrue(CommitClassifier.LineFilter({"value": "gardening", "ratio": 85})('[Gardening] Skip n tests'))
        self.assertTrue(CommitClassifier.LineFilter({"value": "gardening", "ratio": 85})('[girdening] Skip n tests'))

        self.assertFalse(CommitClassifier.LineFilter({"value": "gardening", "ratio": 85})('[gdening] Skip n tests'))

    def test_commit_class_gardening(self):
        c = CommitClassifier(
            name='Gardening',
            pickable=False,
            headers=[dict(value='gardening', ratio=85)],
            paths=[
                'LayoutTests/',
                'Tools/TestWebKitAPI',
            ],
        )
        self.assertEqual(c.name, 'Gardening')
        self.assertFalse(c.pickable)
        self.assertTrue(c.matches('[Gardening] Skip n tests', [], ['LayoutTests/TestExpectations']))

        self.assertFalse(c.matches('[Gardening] Skip n tests', [], []))
        self.assertFalse(c.matches('[gdening] Skip n tests', [], ['LayoutTests/TestExpectations']))
        self.assertFalse(c.matches('[Gardening] Skip n tests', [], ['Makefile', 'LayoutTests/TestExpectations']))

    def test_commit_class_cherry_pick(self):
        c = CommitClassifier(
            name='Cherry-pick',
            pickable=False,
            headers=['^[Cc]herry[- ][Pp]ick'],
            trailers=['^[Oo]riginally[- ]landed[- ]as:'],
        )
        self.assertEqual(c.name, 'Cherry-pick')
        self.assertFalse(c.pickable)
        self.assertTrue(c.matches('Cherry-pick abcde', [], ['somefile']))
        self.assertTrue(c.matches('cherry-pick abcde', [], ['otherfile']))
        self.assertTrue(c.matches('Cherry-Pick abcde', [], []))
        self.assertTrue(c.matches('cherry pick abcde', [], []))
        self.assertTrue(c.matches('[Component] Some change', ['Originally-landed-as: 1234.1@branch (abcde)'], []))

        self.assertFalse(c.matches('Partial cherry-pick', [], []))
        self.assertFalse(c.matches('Took cherry-pick', [], []))

    def test_commit_class_tools(self):
        c = CommitClassifier(
            name='Tools',
            pickable=True,
            paths=['Tools', 'LayoutTests', 'metadata'],
        )
        self.assertEqual(c.name, 'Tools')
        self.assertTrue(c.pickable)
        self.assertTrue(c.matches('Some change', [], ['Tools/Scripts/git-webkit']))
        self.assertTrue(c.matches('', [], ['LayoutTests/some/test.html']))
        self.assertTrue(c.matches('', [], ['metadata/contributors.json']))
        self.assertTrue(c.matches('', [], ['Tools/Scripts/run-webkit-tests', 'metadata/commit_classes.json']))

        self.assertFalse(c.matches('', [], []))
        self.assertFalse(c.matches('', [], ['metadata/contributors.json', 'Makefile']))

    def test_duplicate(self):
        key_a = 'pull_request-3babd344c6c08255c3ce2db7459b57d99209ae43'
        key_b = 'merge_queue-3babd344c6c08255c3ce2db7459b57d99209ae43'

        self.assertFalse(GitHubEventHandlerNoEdits.is_duped(key_a, bucket=1))
        self.assertTrue(GitHubEventHandlerNoEdits.is_duped(key_a, bucket=1))
        self.assertFalse(GitHubEventHandlerNoEdits.is_duped(key_b, bucket=1))

        time.sleep(.5)
        self.assertTrue(GitHubEventHandlerNoEdits.is_duped(key_a, bucket=1))

        time.sleep(2)
        self.assertFalse(GitHubEventHandlerNoEdits.is_duped(key_a, bucket=1))
        self.assertTrue(len(GitHubEventHandlerNoEdits.DUPE_DETECTOR.keys()), 2)

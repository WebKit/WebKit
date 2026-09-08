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

import json
import unittest

import webkitcorepy

from webkitscmpy import agent_review


class MockPullRequest(object):
    def __init__(self, title=None, body=None):
        self.title = title
        self.body = body


# A small but representative review diff: a file header, a hunk header, one
# context line, one removed line and two added lines. Lines carry no trailing
# newline, matching what pull_request.diff(comments=True) yields.
SAMPLE_DIFF = [
    'diff --git a/ChangeLog b/ChangeLog',
    '--- a/ChangeLog',
    '+++ b/ChangeLog',
    '@@ -1,2 +1,3 @@',
    ' context line',
    '-removed line',
    '+added line',
    '+another added',
]


class TestStripLine(unittest.TestCase):
    def test_prefix_and_severity(self):
        self.assertEqual(agent_review.strip_line('agent: [bug] Something is wrong'), 'Something is wrong')

    def test_prefix_without_severity(self):
        self.assertEqual(agent_review.strip_line('agent: Something is wrong'), 'Something is wrong')

    def test_line_without_prefix_untouched(self):
        self.assertEqual(agent_review.strip_line('Not an agent line'), 'Not an agent line')

    def test_continuation_line_untouched(self):
        # Only the first line of a multi-line finding carries the marker; the rest
        # must survive verbatim.
        self.assertEqual(agent_review.strip_line('[bug] not a marker'), '[bug] not a marker')


class TestParseJSON(unittest.TestCase):
    def test_plain_object(self):
        self.assertEqual(agent_review.parse_json('{"comments": []}'), ({'comments': []}, None))

    def test_top_level_array(self):
        value, error = agent_review.parse_json('[1, 2, 3]')
        self.assertEqual(value, [1, 2, 3])
        self.assertIsNone(error)

    def test_empty_text(self):
        self.assertEqual(agent_review.parse_json(''), (None, 'empty text'))
        self.assertEqual(agent_review.parse_json('   \n  '), (None, 'empty text'))

    def test_no_json(self):
        value, error = agent_review.parse_json('not json at all')
        self.assertIsNone(value)
        self.assertTrue(error)

    def test_markdown_fence_rejected(self):
        # The backend asks for schema-validated output, so a fenced or
        # prose-wrapped reply is a failure to report, not something to salvage.
        value, error = agent_review.parse_json('```json\n{"a": 1}\n```')
        self.assertIsNone(value)
        self.assertTrue(error)

    def test_surrounding_prose_rejected(self):
        value, error = agent_review.parse_json('Here you go:\n{"a": 1}\nThanks!')
        self.assertIsNone(value)
        self.assertTrue(error)

    def test_trailing_comma_rejected(self):
        value, error = agent_review.parse_json('{"a": 1,}')
        self.assertIsNone(value)
        self.assertTrue(error)


class TestDiffMaps(unittest.TestCase):
    def test_anchor_and_file_header(self):
        annotated, anchor, file_header = agent_review.diff_maps(SAMPLE_DIFF)
        self.assertEqual(file_header, {'ChangeLog': 2})
        self.assertEqual(anchor, {
            ('destination', 'ChangeLog', 1): 4,
            ('source', 'ChangeLog', 1): 4,
            ('source', 'ChangeLog', 2): 5,
            ('destination', 'ChangeLog', 2): 6,
            ('destination', 'ChangeLog', 3): 7,
        })

    def test_annotation_tags_lines(self):
        annotated, _, _ = agent_review.diff_maps(SAMPLE_DIFF)
        self.assertIn('### File: ChangeLog', annotated)
        self.assertIn('N1   context line', annotated)
        self.assertIn('O2 - removed line', annotated)
        self.assertIn('N2 + added line', annotated)
        self.assertIn('N3 + another added', annotated)

    def test_comment_block_surfaced_but_not_anchored(self):
        diff = [
            '--- a/ChangeLog',
            '+++ b/ChangeLog',
            '@@ -1,1 +1,1 @@',
            '+added line',
            '>>>>',
            'Person: existing comment',
            '<<<<',
        ]
        annotated, anchor, _ = agent_review.diff_maps(diff)
        # The comment is shown to the model as context, but it is not part of the
        # file: it is not line-numbered, does not advance the counter and cannot
        # be anchored to.
        self.assertIn('# prior comment: Person: existing comment', annotated)
        self.assertEqual(anchor, {('destination', 'ChangeLog', 1): 3})

    def test_added_line_resembling_file_header_is_content(self):
        # An added line whose text starts with '++ b/' renders as '+++ b/...';
        # preceded by a non-'--- ' line it must be treated as content, not
        # mistaken for a new file header.
        diff = [
            '--- a/ChangeLog',
            '+++ b/ChangeLog',
            '@@ -1,0 +1,2 @@',
            '+regular added line',
            '+++ b/looks-like-a-header',
        ]
        annotated, anchor, file_header = agent_review.diff_maps(diff)
        self.assertEqual(file_header, {'ChangeLog': 1})
        self.assertEqual(anchor, {
            ('destination', 'ChangeLog', 1): 3,
            ('destination', 'ChangeLog', 2): 4,
        })

    def test_deleted_file_header(self):
        # A deleted file's new-side header is '+++ /dev/null', so its path comes
        # from the old side. Placed first in the diff to catch its content being
        # dropped outright rather than misattributed.
        diff = [
            'diff --git a/Deleted.cpp b/Deleted.cpp',
            '--- a/Deleted.cpp',
            '+++ /dev/null',
            '@@ -1,2 +0,0 @@',
            '-first removed',
            '-second removed',
            'diff --git a/ChangeLog b/ChangeLog',
            '--- a/ChangeLog',
            '+++ b/ChangeLog',
            '@@ -1,0 +1,1 @@',
            '+added line',
        ]
        annotated, anchor, file_header = agent_review.diff_maps(diff)
        self.assertEqual(file_header, {'Deleted.cpp': 2, 'ChangeLog': 8})
        self.assertEqual(anchor, {
            ('source', 'Deleted.cpp', 1): 4,
            ('source', 'Deleted.cpp', 2): 5,
            ('destination', 'ChangeLog', 1): 10,
        })
        self.assertIn('### File: Deleted.cpp', annotated)
        self.assertIn('O1 - first removed', annotated)

    def test_added_file_header(self):
        # An added file's old-side header is '--- /dev/null'; the path comes from
        # the new side.
        diff = [
            '--- /dev/null',
            '+++ b/Added.cpp',
            '@@ -0,0 +1,1 @@',
            '+added line',
        ]
        _, anchor, file_header = agent_review.diff_maps(diff)
        self.assertEqual(file_header, {'Added.cpp': 1})
        self.assertEqual(anchor, {('destination', 'Added.cpp', 1): 3})

    def test_header_pair_without_a_or_b_prefix_is_content(self):
        # Neither side names a path, so this is not a file header and must not
        # reset the file being walked.
        diff = [
            '--- a/ChangeLog',
            '+++ b/ChangeLog',
            '@@ -1,1 +1,2 @@',
            '--- not a header',
            '+++ not a header either',
        ]
        _, anchor, file_header = agent_review.diff_maps(diff)
        self.assertEqual(file_header, {'ChangeLog': 1})
        self.assertEqual(anchor, {
            ('source', 'ChangeLog', 1): 3,
            ('destination', 'ChangeLog', 1): 4,
        })


class TestAnnotateDiff(unittest.TestCase):
    def test_fold_new_side(self):
        findings = [{'file': 'ChangeLog', 'line': 2, 'side': 'new', 'severity': 'bug', 'text': 'This is wrong'}]
        rebuilt, placed, skipped = agent_review.annotate_diff(SAMPLE_DIFF, findings)
        self.assertEqual((placed, skipped), (1, 0))
        self.assertEqual(rebuilt, [
            'diff --git a/ChangeLog b/ChangeLog',
            '--- a/ChangeLog',
            '+++ b/ChangeLog',
            '@@ -1,2 +1,3 @@',
            ' context line',
            '-removed line',
            '+added line',
            'agent: [bug] This is wrong',
            '+another added',
        ])

    def test_fold_old_side(self):
        findings = [{'file': 'ChangeLog', 'line': 2, 'side': 'old', 'text': 'Why remove this?'}]
        rebuilt, placed, skipped = agent_review.annotate_diff(SAMPLE_DIFF, findings)
        self.assertEqual((placed, skipped), (1, 0))
        self.assertEqual(rebuilt[5:7], ['-removed line', 'agent: Why remove this?'])

    def test_file_level_fallback(self):
        findings = [{'file': 'ChangeLog', 'line': 999, 'side': 'new', 'text': 'Anchored to the file'}]
        rebuilt, placed, skipped = agent_review.annotate_diff(SAMPLE_DIFF, findings)
        self.assertEqual((placed, skipped), (1, 1))
        # Falls back to just after the '+++ b/ChangeLog' header (index 2).
        self.assertEqual(rebuilt[2:4], ['+++ b/ChangeLog', 'agent: Anchored to the file'])

    def test_no_findings(self):
        self.assertEqual(agent_review.annotate_diff(SAMPLE_DIFF, []), (None, 0, 0))

    def test_findings_missing_required_fields_ignored(self):
        findings = [
            {'file': 'ChangeLog', 'line': 2, 'side': 'new', 'text': ''},
            {'line': 2, 'side': 'new', 'text': 'No file'},
            'not a dict',
        ]
        self.assertEqual(agent_review.annotate_diff(SAMPLE_DIFF, findings), (None, 0, 0))

    def test_finding_for_unknown_file_dropped_not_counted(self):
        # A finding whose file is not in the diff at all is dropped entirely and
        # must not be counted as a file-level ('not anchored') placement.
        findings = [{'file': 'Nonexistent.cpp', 'line': 5, 'side': 'new', 'text': 'Dropped'}]
        self.assertEqual(agent_review.annotate_diff(SAMPLE_DIFF, findings), (None, 0, 0))

    def test_placed_after_existing_comment_block(self):
        diff = [
            '--- a/ChangeLog',
            '+++ b/ChangeLog',
            '@@ -1,1 +1,1 @@',
            '+added line',
            '>>>>',
            'Person: existing comment',
            '<<<<',
        ]
        findings = [{'file': 'ChangeLog', 'line': 1, 'side': 'new', 'severity': 'nit', 'text': 'See here'}]
        rebuilt, placed, skipped = agent_review.annotate_diff(diff, findings)
        self.assertEqual((placed, skipped), (1, 0))
        self.assertEqual(rebuilt, [
            '--- a/ChangeLog',
            '+++ b/ChangeLog',
            '@@ -1,1 +1,1 @@',
            '+added line',
            '>>>>',
            'Person: existing comment',
            '<<<<',
            'agent: [nit] See here',
        ])

    def test_multiline_text(self):
        findings = [{'file': 'ChangeLog', 'line': 2, 'side': 'new', 'text': 'line one\nline two'}]
        rebuilt, placed, skipped = agent_review.annotate_diff(SAMPLE_DIFF, findings)
        self.assertEqual((placed, skipped), (1, 0))
        self.assertEqual(rebuilt[7:9], ['agent: line one', 'line two'])

    def test_side_mismatch_falls_back_to_file_level(self):
        # The finding claims 'old' but line 3 only exists on the new side. Old and
        # new line numbers are counted independently, so anchoring to the new
        # side's line 3 would be a coincidence: fall back to the file instead, and
        # count it as not anchored.
        findings = [{'file': 'ChangeLog', 'line': 3, 'side': 'old', 'text': 'Which line is this?'}]
        rebuilt, placed, skipped = agent_review.annotate_diff(SAMPLE_DIFF, findings)
        self.assertEqual((placed, skipped), (1, 1))
        self.assertEqual(rebuilt[2:4], ['+++ b/ChangeLog', 'agent: Which line is this?'])


class TestBackend(unittest.TestCase):
    def test_subprocess_wrapper_not_shadowed(self):
        # The module's public run() must not shadow the imported subprocess
        # helper; _run_claude_cli relies on run_command being webkitcorepy.run.
        self.assertIs(agent_review.run_command, webkitcorepy.run)
        self.assertIsNot(agent_review.run, webkitcorepy.run)

    def test_schema_is_serializable(self):
        # The schema is passed to the CLI as a JSON argument.
        self.assertIn('comments', json.loads(json.dumps(agent_review.FINDINGS_SCHEMA))['properties'])

    def test_result_message_from_object(self):
        envelope = {'type': 'result', 'subtype': 'success', 'result': 'text'}
        self.assertEqual(agent_review._result_message(envelope), envelope)

    def test_result_message_from_verbose_array(self):
        # With verbose output enabled the CLI emits every message it produced
        # rather than just the last one, so the result has to be picked out.
        result = {'type': 'result', 'subtype': 'success', 'result': 'text'}
        envelope = [
            {'type': 'system', 'subtype': 'init'},
            {'type': 'assistant', 'message': {}},
            result,
        ]
        self.assertEqual(agent_review._result_message(envelope), result)

    def test_result_message_absent_from_array(self):
        envelope = [{'type': 'system', 'subtype': 'init'}]
        self.assertIsNone(agent_review._result_message(envelope))

    def test_result_message_from_unparsed_envelope(self):
        self.assertIsNone(agent_review._result_message(None))


class TestUserPrompt(unittest.TestCase):
    def test_pull_request_content_and_diff(self):
        annotated, _, _ = agent_review.diff_maps(SAMPLE_DIFF)
        prompt = agent_review._user_prompt(
            MockPullRequest(title='A title', body='A description'), annotated)
        self.assertIn('Title: A title', prompt)
        self.assertIn('A description', prompt)
        self.assertIn('N2 + added line', prompt)

    def test_empty_description(self):
        prompt = agent_review._user_prompt(MockPullRequest(title='A title', body=None), '')
        self.assertIn('(none)', prompt)

    def test_review_instructions_are_not_a_template(self):
        # The PR content is the user prompt, so nothing an author writes can be
        # substituted into the instructions.
        self.assertNotIn('{', agent_review.REVIEW_INSTRUCTIONS)


if __name__ == '__main__':
    unittest.main()

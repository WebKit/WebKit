#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
angle_presubmit_utils_unittest.py: Top-level unittest script for ANGLE presubmit checks.
"""

import importlib.machinery
import os
import pathlib
import sys
import tempfile
import unittest
from angle_presubmit_utils import *

def SetCWDToAngleFolder():
    angle_folder = pathlib.Path(__file__).resolve().parents[1]
    os.chdir(angle_folder)


SetCWDToAngleFolder()

loader = importlib.machinery.SourceFileLoader('PRESUBMIT', 'PRESUBMIT.py')
PRESUBMIT = loader.load_module()



class CommitMessageFormattingCheckTest(unittest.TestCase):

    def __init__(self, *args, **kwargs):
        super(CommitMessageFormattingCheckTest, self).__init__(*args, **kwargs)
        self.output_api = OutputAPI_mock()

    def run_check_commit_message_formatting(self, commit_msg):
        input_api = InputAPI_mock(commit_msg)
        return PRESUBMIT._CheckCommitMessageFormatting(input_api, self.output_api)

    def test_correct_commit_message(self):
        commit_msg = """a

b

Bug: angleproject:42263262
Change-Id: I966c79d96175da9eee92ef6da20db50d488137b2
"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 0)

    def test_missing_description_body_and_description_summary(self):
        commit_msg = """Change-Id: I966c79d96175da9eee92ef6da20db50d488137b2"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 1)
        self.assertEqual(
            errors[0],
            self.output_api.PresubmitError(
                "Commit 1:Please ensure that your" +
                " description summary and description body are not blank."))

    def test_missing_description_body(self):
        commit_msg = """
        a

b: d
c: e
"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 0)

    def test_missing_tag_paragraph(self):
        commit_msg = """a

bd
efgh"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 1)
        self.assertEqual(
            errors[0],
            self.output_api.PresubmitError(
                "Commit 1:Please ensure that there are tags (e.g., Bug:, Test:) in your description."
            ))

    def test_missing_tag_paragraph_and_description_body(self):
        commit_msg = "a"
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 1)
        self.assertEqual(
            errors[0],
            self.output_api.PresubmitError(
                "Commit 1:Please ensure that there are tags (e.g., Bug:, Test:) in your description."
            ))

    def test_missing_blank_line_between_description_summary_and_description_body(self):
        commit_msg = """a
b

Change-Id: I925cdb45779a9cdebe4e14f9e81e4211ade37c12"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 1)
        self.assertEqual(errors[0], self.output_api.PresubmitError(
          "Commit 1:Please ensure the summary is only 1 line and there is 1 blank line" + \
          " between the summary and description body."))

    def test_missing_blank_line_between_description_body_and_tags_paragraph(self):
        commit_msg = """a

b
Change-Id: I925cdb45779a9cdebe4e14f9e81e4211ade37c12"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 0)

    def test_multiple_blank_lines_before_and_after_commit_message(self):
        commit_msg = """


                a

                  b

Change-Id: I925cdb45779a9cdebe4e14f9e81e4211ade37c12
"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 0)

    def test_newlines_within_description_body(self):
        commit_msg = """a

b

d

e

for

Change-Id: I443c36aaa8956c20da1abddf7aea613659e2cd5b"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 0)

    # Summary description in warning threshold(at 65 characters)
    def test_summmary_description_in_warning_thresholds(self):
        commit_msg = """aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa

b

Change-Id: I925cdb45779a9cdebe4e14f9e81e4211ade37c12
"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 1)
        self.assertEqual(
            errors[0],
            self.output_api.PresubmitPromptWarning(
                "Commit 1:Your description summary should be on one line of 64 or less characters."
            ))

    # Summary description in error threshold(at 71 characters)
    def test_summary_description_in_error_threshold(self):
        commit_msg = """aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa

b

Change-Id: I925cdb45779a9cdebe4e14f9e81e4211ade37c12"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 1)
        self.assertEqual(
            errors[0],
            self.output_api.PresubmitError(
                "Commit 1:Please ensure that your description summary is on one line of 64 or less characters."
            ))

    def test_description_body_exceeds_line_count_limit(self):
        commit_msg = """a

bbbbbbbb bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb


Change-Id: I443c36aaa8956c20da1abddf7aea613659e2cd5b"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 2)
        self.assertEqual(
            errors[0],
            self.output_api.PresubmitError(
                "Commit 1:Please ensure that there exists only 1 blank line between tags and description body."
            ))
        self.assertEqual(
            errors[1],
            self.output_api.PresubmitError("""Commit 1:Line 3 is too long.
"bbbbbbbb bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
Please wrap it to 72 characters. Lines without spaces or lines starting with 4 spaces are exempt."""
                                          ))

    def test_description_body_exceeds_line_count_limit_but_with_4_spaces_prefix(self):
        commit_msg = """a

cc

dddd

    bbbbbbbbbbbbbbbbbbbbbbb bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb

Change-Id: I443c36aaa8956c20da1abddf7aea613659e2cd5b"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 0)

    def test_description_body_exceeds_line_count_limit_but_without_space(self):
        commit_msg = """a

cc

dddd

bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb

a: d"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 0)

    def test_description_body_exceeds_line_count_limit_but_in_quote(self):
        commit_msg = """a

cc

dddd

> bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb

a: d"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 0)

    def test_tabs_in_commit_message(self):
        commit_msg = """																a

bbbbbbbbbbbbbbbbbbbb

Change-Id: I443c36aaa8956c20da1abddf7aea613659e2cd5b"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 1)
        self.assertEqual(
            errors[0],
            self.output_api.PresubmitError("Commit 1:Tabs are not allowed in commit message."))

    def test_allowlist_revert(self):
        commit_msg = """Revert "sssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssa

bbbbbbbbbbbbbbbbbbbb

Change-Id: I443c36aaa8956c20da1abddf7aea613659e2cd5b"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 0)

    def test_allowlist_roll(self):
        commit_msg = """Roll sssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssadd

bbbbbbbbbbbbbbbbbbbb

Change-Id: I443c36aaa8956c20da1abddf7aea613659e2cd5b"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 0)

    def test_allowlist_reland(self):
        commit_msg = """Reland sssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssadd

bbbbbbbbbbbbbbbbbbbb

Change-Id: I443c36aaa8956c20da1abddf7aea613659e2cd5b"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 0)

    def test_allowlist_reland2(self):
        commit_msg = """Reland: sssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssadd

bbbbbbbbbbbbbbbbbbbb

Change-Id: I443c36aaa8956c20da1abddf7aea613659e2cd5b"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 0)

    def test_multiple_commits_with_errors_in_multiple_commits(self):
        commit_msg = """a

bbbbbbbb bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb

Change-Id: I443c36aaa8956c20da1abddf7aea613659e2cd5b

a

cccccccccccccccccccccccccccccc cccccccccccccccccccccccccccccccccccccccccccc

Change-Id: I443c36aaa8956c20da1abddf7aea613659e2cd5b"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 2)
        self.assertEqual(
            errors[0],
            self.output_api.PresubmitError("""Commit 2:Line 3 is too long.
"bbbbbbbb bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
Please wrap it to 72 characters. Lines without spaces or lines starting with 4 spaces are exempt."""
                                          ))
        self.assertEqual(
            errors[1],
            self.output_api.PresubmitError("""Commit 1:Line 4 is too long.
"cccccccccccccccccccccccccccccc cccccccccccccccccccccccccccccccccccccccccccc"
Please wrap it to 72 characters. Lines without spaces or lines starting with 4 spaces are exempt."""
                                          ))

    def test_multiple_commits_with_error_in_one_commit(self):
        commit_msg = """a

bbbbbbbb bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb

Change-Id: I443c36aaa8956c20da1abddf7aea613659e2cd5b

Roll sssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssadd

bbbbbbbbbbbbbbbbbbbb

Change-Id: I443c36aaa8956c20da1abddf7aea613659e2cd5b"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 1)
        self.assertEqual(
            errors[0],
            self.output_api.PresubmitError("""Commit 2:Line 3 is too long.
"bbbbbbbb bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
Please wrap it to 72 characters. Lines without spaces or lines starting with 4 spaces are exempt."""
                                          ))

    def test_multiple_commits_with_no_error(self):
        commit_msg = """Reland sssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssadd

bbbbbbbbbbbbbbbbbbbb

Change-Id: I443c36aaa8956c20da1abddf7aea613659e2cd5b

Roll sssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssadd

bbbbbbbbbbbbbbbbbbbb

Change-Id: I443c36aaa8956c20da1abddf7aea613659e2cd5b"""
        errors = self.run_check_commit_message_formatting(commit_msg)
        self.assertEqual(len(errors), 0)


class GClientFileExistenceCheck(unittest.TestCase):

    def __init__(self, *args, **kwargs):
        super(GClientFileExistenceCheck, self).__init__(*args, **kwargs)
        self.output_api = OutputAPI_mock()

    def run_gclient_presubmit(self, cwd, search_limit):
        input_api = InputAPI_mock('')
        input_api.cwd = cwd
        return PRESUBMIT._CheckGClientExists(input_api, self.output_api,
                                             pathlib.Path(search_limit))

    def test_gclient_in_cwd(self):
        with tempfile.TemporaryDirectory() as cwd:
            pathlib.Path(cwd).joinpath('.gclient').touch()
            errors = self.run_gclient_presubmit(cwd, cwd)
            self.assertEqual(len(errors), 0)

    def test_missing_gclient(self):
        with tempfile.TemporaryDirectory() as cwd:
            errors = self.run_gclient_presubmit(cwd, cwd)
            self.assertEqual(len(errors), 1)
            self.assertEqual(errors[0], self.output_api.PresubmitError('Missing .gclient file.'))

    def test_gclient_in_parent(self):
        with tempfile.TemporaryDirectory() as cwd:
            cwd = pathlib.Path(cwd)
            cwd.joinpath('.gclient').touch()
            inner = cwd.joinpath('inner')
            inner.mkdir()
            errors = self.run_gclient_presubmit(str(inner), str(cwd))
            self.assertEqual(len(errors), 0)


class CheckShaderVersionInShaderLangHeaderTest(unittest.TestCase):

    def __init__(self, *args, **kwargs):
        super(CheckShaderVersionInShaderLangHeaderTest, self).__init__(*args, **kwargs)
        self.output_api = OutputAPI_mock()

    def run_shader_version_check_presubmit(self, commit_msg, diffs):
        affected_files = [AffectedFile_mock(diff) for diff in diffs]
        input_api = InputAPI_mock(commit_msg, affected_files)
        return PRESUBMIT._CheckShaderVersionInShaderLangHeader(input_api, self.output_api)

    def test_headers_not_changed(self):
        errors = self.run_shader_version_check_presubmit('', [])
        self.assertEqual(len(errors), 0)

    def test_shader_lang_changed_with_version_change(self):
        shader_lang_diff = """-#define ANGLE_SH_VERSION 100
+#define ANGLE_SH_VERSION 101
"""

        errors = self.run_shader_version_check_presubmit('', [shader_lang_diff])
        self.assertEqual(len(errors), 0)

    def test_both_changed_with_version_change(self):
        shader_lang_diff = """-#define ANGLE_SH_VERSION 100
+#define ANGLE_SH_VERSION 101
"""
        shader_vars_diff = """-any change"""

        errors = self.run_shader_version_check_presubmit('', [shader_lang_diff, shader_vars_diff])
        self.assertEqual(len(errors), 0)

    def test_shader_lang_changed_with_no_version_change(self):
        shader_lang_diff = """+some change"""

        errors = self.run_shader_version_check_presubmit('', [shader_lang_diff])
        self.assertEqual(len(errors), 1)
        self.assertEqual(
            errors[0],
            self.output_api.PresubmitError(
                'ANGLE_SH_VERSION should be incremented when ShaderLang.h or ShaderVars.h change.')
        )

    def test_shader_lang_changed_with_no_version_change(self):
        shader_lang_diff = """+some change
 #define ANGLE_SH_VERSION 100
-other changes"""

        errors = self.run_shader_version_check_presubmit('', [shader_lang_diff])
        self.assertEqual(len(errors), 1)
        self.assertEqual(
            errors[0],
            self.output_api.PresubmitError(
                'ANGLE_SH_VERSION should be incremented when ShaderLang.h or ShaderVars.h change.')
        )

    def test_shader_lang_changed_with_version_cosmetic_change(self):
        shader_lang_diff = """-#define ANGLE_SH_VERSION 100
+#define ANGLE_SH_VERSION 100 // cosmetic change
"""

        errors = self.run_shader_version_check_presubmit('', [shader_lang_diff])
        self.assertEqual(len(errors), 1)
        self.assertEqual(
            errors[0],
            self.output_api.PresubmitError(
                'ANGLE_SH_VERSION should be incremented when ShaderLang.h or ShaderVars.h change.')
        )

    def test_shader_lang_changed_with_version_decrement(self):
        shader_lang_diff = """-#define ANGLE_SH_VERSION 100
+#define ANGLE_SH_VERSION 99
"""

        errors = self.run_shader_version_check_presubmit('', [shader_lang_diff])
        self.assertEqual(len(errors), 1)
        self.assertEqual(
            errors[0],
            self.output_api.PresubmitError(
                'ANGLE_SH_VERSION should be incremented when ShaderLang.h or ShaderVars.h change.')
        )

    def test_shader_lang_changed_in_revert(self):
        shader_lang_diff = """-#define ANGLE_SH_VERSION 100
+#define ANGLE_SH_VERSION 99
"""

        errors = self.run_shader_version_check_presubmit('Revert some change', [shader_lang_diff])
        self.assertEqual(len(errors), 0)


class RestrictedTracesCheckTest(unittest.TestCase):

    def __init__(self, *args, **kwargs):
        super(RestrictedTracesCheckTest, self).__init__(*args, **kwargs)
        self.output_api = OutputAPI_mock()
        self.angle_root = os.getcwd()

    def setUp(self):
        json_path = os.path.join(self.angle_root,
                                 'src/tests/restricted_traces/restricted_traces.json')
        with open(json_path, 'r') as f:
            self.original_content = f.read().splitlines()

    def run_check(self, affected_files):
        input_api = InputAPI_mock('', affected_files=affected_files)
        input_api.cwd = self.angle_root
        return PRESUBMIT._CheckRestrictedTraces(input_api, self.output_api)

    def test_json_not_modified(self):
        affected_files = [AffectedFile_mock('', 'some/other/file.cpp')]
        errors = self.run_check(affected_files)
        self.assertEqual(len(errors), 0)

    def test_json_modified_limit_not_exceeded(self):
        affected_files = [
            AffectedFile_mock(
                '',
                'src/tests/restricted_traces/restricted_traces.json',
                old_contents=self.original_content)
        ]
        errors = self.run_check(affected_files)
        self.assertEqual(len(errors), 0)

    def test_json_modified_limit_exceeded(self):
        json_path = os.path.join(self.angle_root,
                                 'src/tests/restricted_traces/restricted_traces.json')
        with open(json_path, 'r') as f:
            original_data = json.load(f)

        try:
            temp_data = original_data.copy()
            # Add 11 extra traces (no 'ci' and no 'representative')
            temp_data['traces'] = original_data['traces'] + [
                f"dummy_trace_{i} 1" for i in range(11)
            ]
            with open(json_path, 'w') as f:
                json.dump(temp_data, f)

            affected_files = [
                AffectedFile_mock(
                    '',
                    'src/tests/restricted_traces/restricted_traces.json',
                    old_contents=self.original_content)
            ]
            errors = self.run_check(affected_files)
            self.assertEqual(len(errors), 1)
            self.assertIn("Too many CQ extra traces", errors[0]._message)
        finally:
            with open(json_path, 'w') as f:
                json.dump(original_data, f, indent=2)
                f.write('\n')

    def test_missing_traces_key(self):
        json_path = os.path.join(self.angle_root,
                                 'src/tests/restricted_traces/restricted_traces.json')
        with open(json_path, 'r') as f:
            original_data = json.load(f)

        try:
            temp_data = original_data.copy()
            if 'traces' in temp_data:
                del temp_data['traces']
            with open(json_path, 'w') as f:
                json.dump(temp_data, f)

            affected_files = [
                AffectedFile_mock(
                    '',
                    'src/tests/restricted_traces/restricted_traces.json',
                    old_contents=self.original_content)
            ]
            errors = self.run_check(affected_files)
            self.assertEqual(len(errors), 1)
            self.assertIn("missing the \"traces\" key", errors[0]._message)
        finally:
            with open(json_path, 'w') as f:
                json.dump(original_data, f, indent=2)
                f.write('\n')

    def test_allowed_coexisting_tags(self):
        json_path = os.path.join(self.angle_root,
                                 'src/tests/restricted_traces/restricted_traces.json')
        with open(json_path, 'r') as f:
            original_data = json.load(f)

        try:
            temp_data = original_data.copy()
            temp_data['traces'] = original_data['traces'] + [
                "dummy_trace_1 1 ci representative smoke"
            ]

            with open(json_path, 'w') as f:
                json.dump(temp_data, f)

            with open(json_path, 'r') as f:
                mutated_content = f.read().splitlines()
            affected_files = [
                AffectedFile_mock(
                    '',
                    'src/tests/restricted_traces/restricted_traces.json',
                    old_contents=mutated_content)
            ]
            errors = self.run_check(affected_files)
            self.assertEqual(len(errors), 0)
        finally:
            with open(json_path, 'w') as f:
                json.dump(original_data, f, indent=2)
                f.write('\n')

    def test_unsorted_tags(self):
        json_path = os.path.join(self.angle_root,
                                 'src/tests/restricted_traces/restricted_traces.json')
        with open(json_path, 'r') as f:
            original_data = json.load(f)

        try:
            temp_data = original_data.copy()
            new_traces = []
            modified = False
            for trace in original_data['traces']:
                if 'antutu_refinery' in trace and not modified:
                    new_traces.append("antutu_refinery 1 representative ci smoke")
                    modified = True
                else:
                    new_traces.append(trace)
            temp_data['traces'] = new_traces

            with open(json_path, 'w') as f:
                json.dump(temp_data, f)

            affected_files = [
                AffectedFile_mock(
                    '',
                    'src/tests/restricted_traces/restricted_traces.json',
                    old_contents=self.original_content)
            ]
            errors = self.run_check(affected_files)
            self.assertEqual(len(errors), 1)
            self.assertIn("has unsorted tags", errors[0]._message)
        finally:
            with open(json_path, 'w') as f:
                json.dump(original_data, f, indent=2)
                f.write('\n')

    def test_new_trace_with_tags_fails(self):
        json_path = os.path.join(self.angle_root,
                                 'src/tests/restricted_traces/restricted_traces.json')
        with open(json_path, 'r') as f:
            original_data = json.load(f)

        try:
            temp_data = original_data.copy()
            temp_data['traces'] = original_data['traces'] + ["dummy_new_trace 1 representative"]

            with open(json_path, 'w') as f:
                json.dump(temp_data, f)

            affected_files = [
                AffectedFile_mock(
                    '',
                    'src/tests/restricted_traces/restricted_traces.json',
                    old_contents=self.original_content)
            ]
            errors = self.run_check(affected_files)
            self.assertEqual(len(errors), 1)
            self.assertIn("must not have any tags initially", errors[0]._message)
        finally:
            with open(json_path, 'w') as f:
                json.dump(original_data, f, indent=2)
                f.write('\n')

    def test_new_trace_without_tags_passes(self):
        json_path = os.path.join(self.angle_root,
                                 'src/tests/restricted_traces/restricted_traces.json')
        with open(json_path, 'r') as f:
            original_data = json.load(f)

        try:
            temp_data = original_data.copy()
            temp_data['traces'] = original_data['traces'] + ["dummy_new_trace 1"]

            with open(json_path, 'w') as f:
                json.dump(temp_data, f)

            affected_files = [
                AffectedFile_mock(
                    '',
                    'src/tests/restricted_traces/restricted_traces.json',
                    old_contents=self.original_content)
            ]
            errors = self.run_check(affected_files)
            self.assertEqual(len(errors), 0)
        finally:
            with open(json_path, 'w') as f:
                json.dump(original_data, f, indent=2)
                f.write('\n')


class CheckUnsafeBuffersSafetyCommentsTest(unittest.TestCase):

    def testNoUsage(self):
        mock_input_api = InputAPI_mock('')
        mock_input_api.source_files = [
            AffectedFile_mock(
                '', 'src/libANGLE/Foo.cpp', new_contents=[
                    'void Foo() {',
                    '    int x = 0;',
                    '}',
                ])
        ]
        errors = PRESUBMIT._CheckUnsafeBuffersSafetyComments(mock_input_api, OutputAPI_mock())
        self.assertEqual(0, len(errors))

    def testValidUsageSameLine(self):
        mock_input_api = InputAPI_mock('')
        mock_input_api.source_files = [
            AffectedFile_mock(
                '',
                'src/libANGLE/Foo.cpp',
                new_contents=[
                    'void Foo() {',
                    '    ANGLE_UNSAFE_BUFFERS(ptr[0] = 0); // SAFETY: safe',
                    '}',
                ])
        ]
        errors = PRESUBMIT._CheckUnsafeBuffersSafetyComments(mock_input_api, OutputAPI_mock())
        self.assertEqual(0, len(errors))

    def testValidUsagePrecedingLine(self):
        mock_input_api = InputAPI_mock('')
        mock_input_api.source_files = [
            AffectedFile_mock(
                '',
                'src/libANGLE/Foo.cpp',
                new_contents=[
                    'void Foo() {',
                    '    // SAFETY: safe',
                    '    ANGLE_UNSAFE_BUFFERS(ptr[0] = 0);',
                    '}',
                ])
        ]
        errors = PRESUBMIT._CheckUnsafeBuffersSafetyComments(mock_input_api, OutputAPI_mock())
        self.assertEqual(0, len(errors))

    def testValidUsagePrecedingLineWithOtherComments(self):
        mock_input_api = InputAPI_mock('')
        mock_input_api.source_files = [
            AffectedFile_mock(
                '',
                'src/libANGLE/Foo.cpp',
                new_contents=[
                    'void Foo() {',
                    '    // SAFETY: safe',
                    '    // some other info',
                    '    ANGLE_UNSAFE_BUFFERS(ptr[0] = 0);',
                    '}',
                ])
        ]
        errors = PRESUBMIT._CheckUnsafeBuffersSafetyComments(mock_input_api, OutputAPI_mock())
        self.assertEqual(0, len(errors))

    def testValidUsageBrokenLine(self):
        mock_input_api = InputAPI_mock('')
        mock_input_api.source_files = [
            AffectedFile_mock(
                '',
                'src/libANGLE/Foo.cpp',
                new_contents=[
                    'void Foo() {',
                    '    // SAFETY: safe',
                    '    int very_long_line_due_lots_of_stuff = line_broken_by_clang_format +',
                    '    ANGLE_UNSAFE_BUFFERS(ptr[0] = 0);',
                    '}',
                ])
        ]
        errors = PRESUBMIT._CheckUnsafeBuffersSafetyComments(mock_input_api, OutputAPI_mock())
        self.assertEqual(0, len(errors))

    def testValidUsageSafetyCommentWithCodeSyntax(self):
        mock_input_api = InputAPI_mock('')
        mock_input_api.source_files = [
            AffectedFile_mock(
                '',
                'src/libANGLE/Foo.cpp',
                new_contents=[
                    'void Foo() {',
                    '    // SAFETY: Explanation too large to desplay in one line of comment and use of c++ syntax',
                    '    // to justify usage.... This is safe because',
                    '    // dummy[1] = dummy[0]++; is safe',
                    '    ANGLE_UNSAFE_BUFFERS(ptr[0] = 0);',
                    '}',
                ])
        ]
        errors = PRESUBMIT._CheckUnsafeBuffersSafetyComments(mock_input_api, OutputAPI_mock())
        self.assertEqual(0, len(errors))

    def testInvalidUsageCommentSeparatedByForLoop(self):
        mock_input_api = InputAPI_mock('')
        mock_input_api.source_files = [
            AffectedFile_mock(
                '',
                'src/libANGLE/Foo.cpp',
                new_contents=[
                    'void Foo() {',
                    '    // SAFETY: safe',
                    '    for(int b = 0; b < buffer_size; b++){',
                    '       ANGLE_UNSAFE_BUFFERS(dummy[b] += x);',
                    '    }'
                    '}',
                ])
        ]
        errors = PRESUBMIT._CheckUnsafeBuffersSafetyComments(mock_input_api, OutputAPI_mock())
        self.assertEqual(1, len(errors))

    def testInvalidUsageNoComment(self):
        mock_input_api = InputAPI_mock('')
        mock_input_api.source_files = [
            AffectedFile_mock(
                '',
                'src/libANGLE/Foo.cpp',
                new_contents=[
                    'void Foo() {',
                    '    ANGLE_UNSAFE_BUFFERS(ptr[0] = 0);',
                    '}',
                ])
        ]
        errors = PRESUBMIT._CheckUnsafeBuffersSafetyComments(mock_input_api, OutputAPI_mock())
        self.assertEqual(1, len(errors))
        self.assertIn('ANGLE_UNSAFE_BUFFERS usage must be accompanied', errors[0].items[0])

    def testInvalidUsageCommentNotSafety(self):
        mock_input_api = InputAPI_mock('')
        mock_input_api.source_files = [
            AffectedFile_mock(
                '',
                'src/libANGLE/Foo.cpp',
                new_contents=[
                    'void Foo() {',
                    '    // this is a comment but not safety',
                    '    ANGLE_UNSAFE_BUFFERS(ptr[0] = 0);',
                    '}',
                ])
        ]
        errors = PRESUBMIT._CheckUnsafeBuffersSafetyComments(mock_input_api, OutputAPI_mock())
        self.assertEqual(1, len(errors))

    def testInvalidUsageCommentSeparatedByCode(self):
        mock_input_api = InputAPI_mock('')
        mock_input_api.source_files = [
            AffectedFile_mock(
                '',
                'src/libANGLE/Foo.cpp',
                new_contents=[
                    'void Foo() {',
                    '    // SAFETY: safe',
                    '    int x = 0;',
                    '    ANGLE_UNSAFE_BUFFERS(ptr[0] = 0);',
                    '}',
                ])
        ]
        errors = PRESUBMIT._CheckUnsafeBuffersSafetyComments(mock_input_api, OutputAPI_mock())
        self.assertEqual(1, len(errors))

    def testIgnoreCommentedUsage(self):
        mock_input_api = InputAPI_mock('')
        mock_input_api.source_files = [
            AffectedFile_mock(
                '',
                'src/libANGLE/Foo.cpp',
                new_contents=[
                    'void Foo() {',
                    '    // ANGLE_UNSAFE_BUFFERS(ptr[0] = 0);',
                    '}',
                ])
        ]
        errors = PRESUBMIT._CheckUnsafeBuffersSafetyComments(mock_input_api, OutputAPI_mock())
        self.assertEqual(0, len(errors))


class CheckBannedPatternsTest(unittest.TestCase):

    def testNoUsage(self):
        mock_input_api = InputAPI_mock('')
        mock_input_api.source_files = [
            AffectedFile_mock(
                '', 'src/libANGLE/Foo.cpp', new_contents=[
                    'void Foo() {',
                    '    int x = 0;',
                    '}',
                ])
        ]
        errors = PRESUBMIT._CheckNoBannedPatterns(mock_input_api, OutputAPI_mock())
        self.assertEqual(0, len(errors))

    def testBannedUnsafeTodo(self):
        mock_input_api = InputAPI_mock('')
        mock_input_api.source_files = [
            AffectedFile_mock(
                '',
                'src/libANGLE/Foo.cpp',
                new_contents=[
                    'void Foo() {',
                    '    ANGLE_UNSAFE_TODO(ptr[0] = 0);',
                    '}',
                ])
        ]
        errors = PRESUBMIT._CheckNoBannedPatterns(mock_input_api, OutputAPI_mock())
        self.assertEqual(1, len(errors))
        self.assertIn('A banned pattern was used.', errors[0]._message)
        self.assertIn('src/libANGLE/Foo.cpp:2:', errors[0]._message)
        self.assertIn('Do not introduce new instances of ANGLE_UNSAFE_TODO', errors[0]._message)
        self.assertEqual(1, len(errors[0].locations))
        loc = errors[0].locations[0]
        self.assertEqual('src/libANGLE/Foo.cpp', loc.file_path)
        self.assertEqual(2, loc.start_line)
        self.assertEqual(2, loc.end_line)

    def testBannedPragma(self):
        mock_input_api = InputAPI_mock('')
        mock_input_api.source_files = [
            AffectedFile_mock(
                '', 'src/libANGLE/Foo.cpp', new_contents=[
                    '#pragma allow_unsafe_buffers',
                ])
        ]
        errors = PRESUBMIT._CheckNoBannedPatterns(mock_input_api, OutputAPI_mock())
        self.assertEqual(1, len(errors))
        self.assertIn('A banned pattern was used.', errors[0]._message)
        self.assertIn('src/libANGLE/Foo.cpp:1:', errors[0]._message)
        self.assertIn('#pragma allow_unsafe_buffers is discouraged', errors[0]._message)
        self.assertEqual(1, len(errors[0].locations))
        loc = errors[0].locations[0]
        self.assertEqual('src/libANGLE/Foo.cpp', loc.file_path)
        self.assertEqual(1, loc.start_line)
        self.assertEqual(1, loc.end_line)

    def testCommentedUsageIgnored(self):
        mock_input_api = InputAPI_mock('')
        mock_input_api.source_files = [
            AffectedFile_mock(
                '',
                'src/libANGLE/Foo.cpp',
                new_contents=[
                    '// ANGLE_UNSAFE_TODO(ptr[0] = 0);',
                    '// #pragma allow_unsafe_buffers',
                ])
        ]
        errors = PRESUBMIT._CheckNoBannedPatterns(mock_input_api, OutputAPI_mock())
        self.assertEqual(0, len(errors))

if __name__ == '__main__':
    unittest.main()

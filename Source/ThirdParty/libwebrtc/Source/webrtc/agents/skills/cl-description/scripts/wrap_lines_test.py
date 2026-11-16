#!/usr/bin/env vpython3

# Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# pylint: disable=line-too-long
"""Tests for wrap_lines.py"""

import textwrap
import unittest
from wrap_lines import wrap_text


class WrapLinesTest(unittest.TestCase):
    """Tests for wrap_text function."""

    def test_formatting(self):
        """Test various CL description formatting draft & final cases."""
        test_cases = [
            {
                "name": "empty_text",
                "draft": "   ",
                "final": "",
            },
            {
                "name": "single_line_subject",
                "draft": "[Component] Simple subject line",
                "final": "[Component] Simple subject line",
            },
            {
                "name":
                "short_subject_and_body",
                "draft":
                """
                    [Component] Subject
                    Short body line.
                """,
                "final":
                """
                    [Component] Subject

                    Short body line.
                """,
            },
            {
                "name":
                "long_body_lines_wrapping",
                "width":
                72,
                "draft":
                """
                    [Component] Subject
                    This is a very long line that should be wrapped to seventy-two characters by the script because it exceeds the length limit.
                """,
                "final":
                """
                    [Component] Subject

                    This is a very long line that should be wrapped to seventy-two
                    characters by the script because it exceeds the length limit.
                """,
            },
            {
                "name":
                "multiple_paragraphs",
                "draft":
                """
                    [Comp] Subject
                    Paragraph one with some text.

                    Paragraph two with some more text.
                """,
                "final":
                """
                    [Comp] Subject

                    Paragraph one with some text.

                    Paragraph two with some more text.
                """,
            },
            {
                "name":
                "subject_paragraph_and_bullets",
                "width":
                72,
                "draft":
                """
                    [Comp] Subject
                    This is an introductory paragraph explaining the overall change.

                    - First bullet point with details that are quite long and need to wrap properly.
                    - Second bullet point
                """,
                "final":
                """
                    [Comp] Subject

                    This is an introductory paragraph explaining the overall change.

                    - First bullet point with details that are quite long and need to wrap
                      properly.
                    - Second bullet point
                """,
            },
            {
                "name":
                "bullet_points_wrapping",
                "width":
                72,
                "draft":
                """
                    [Comp] Subject
                    - First bullet point that is quite long and exceeds the seventy-two character limit so it needs to wrap properly.
                    - Second short bullet.
                """,
                "final":
                """
                    [Comp] Subject

                    - First bullet point that is quite long and exceeds the seventy-two
                      character limit so it needs to wrap properly.
                    - Second short bullet.
                """,
            },
            {
                "name":
                "bullet_custom_indent",
                "width":
                40,
                "draft":
                """
                    [Comp] Subject
                    *   Or other indentation sizes
                         are OK - the formatter should
                         be flexible with whatever the
                         author indented as
                """,
                "final":
                """
                    [Comp] Subject

                    *   Or other indentation sizes are OK -
                        the formatter should be flexible
                        with whatever the author indented as
                """,
            },
            {
                "name":
                "various_bullet_types",
                "width":
                50,
                "draft":
                """
                    [Comp] Subject
                    * Asterisk bullet point that is long enough to require wrapping onto line two.
                    + Plus bullet point that is long enough to require wrapping onto line two.
                    1. Numbered list item that is long enough to require wrapping onto line two.
                    2) Paren numbered item that is long enough to require wrapping onto line two.
                """,
                "final":
                """
                    [Comp] Subject

                    * Asterisk bullet point that is long enough to
                      require wrapping onto line two.
                    + Plus bullet point that is long enough to require
                      wrapping onto line two.
                    1. Numbered list item that is long enough to
                       require wrapping onto line two.
                    2) Paren numbered item that is long enough to
                       require wrapping onto line two.
                """,
            },
            {
                "name":
                "footers_preserved",
                "draft":
                """
                    [Comp] Subject
                    Some description text.

                    Bug: 123456
                    Test: manual verification
                    Change-Id: I123456
                """,
                "final":
                """
                    [Comp] Subject

                    Some description text.

                    Bug: 123456
                    Test: manual verification
                    Change-Id: I123456
                """,
            },
            {
                "name":
                "obsolete_histogram_tag_single_line",
                "draft":
                """
                    [Comp] Subject
                    Some description text explaining the rationale for the changes.

                    OBSOLETE_HISTOGRAM[Tab.VeryLongHistogramName]=Replaced by Tab.AnotherLongHistogramName2 because the original metric is no longer needed.
                    Bug: 123456
                    Test: manual verification
                """,
                "final":
                """
                    [Comp] Subject

                    Some description text explaining the rationale for the changes.

                    OBSOLETE_HISTOGRAM[Tab.VeryLongHistogramName]=Replaced by Tab.AnotherLongHistogramName2 because the original metric is no longer needed.
                    Bug: 123456
                    Test: manual verification
                """,
            },
            {
                "name":
                "subject_ends_with_single_period",
                "draft":
                """
                    [Comp] Subject with period.
                    Some description.
                """,
                "final":
                """
                    [Comp] Subject with period

                    Some description.
                """,
            },
            {
                "name":
                "subject_ends_with_ellipsis",
                "draft":
                """
                    [Comp] Subject with ellipsis...
                    Some description.
                """,
                "final":
                """
                    [Comp] Subject with ellipsis...

                    Some description.
                """,
            },
            {
                "name":
                "webrtc_bug_footers_single_line",
                "draft":
                """
                    pc: Subject
                    Some description text.

                    Bug: webrtc:123456, webrtc:234567, chromium:345678, chromium:456789, b/567890
                    Change-Id: I123456
                """,
                "final":
                """
                    pc: Subject

                    Some description text.

                    Bug: webrtc:123456, webrtc:234567, chromium:345678, chromium:456789, b/567890
                    Change-Id: I123456
                """,
            },
            {
                "name":
                "body_line_with_colon_is_not_a_footer",
                "width":
                40,
                "draft":
                """
                    [Comp] Subject
                    Note: this body paragraph starts with a word and a colon.

                    Bug: none
                """,
                "final":
                """
                    [Comp] Subject

                    Note: this body paragraph starts with a
                    word and a colon.

                    Bug: none
                """,
            },
            {
                "name":
                "indented_block_preserved",
                "draft":
                """
                    [Comp] Subject
                    Crash signature:

                        foo()   at foo.cc:12
                        bar()   at bar.cc:34
                """,
                "final":
                """
                    [Comp] Subject

                    Crash signature:

                        foo()   at foo.cc:12
                        bar()   at bar.cc:34
                """,
            },
            {
                "name":
                "long_url_not_broken",
                "width":
                40,
                "draft":
                """
                    [Comp] Subject
                    See https://webrtc-review.googlesource.com/c/src/+/123456 for details.
                """,
                "final":
                """
                    [Comp] Subject

                    See
                    https://webrtc-review.googlesource.com/c/src/+/123456
                    for details.
                """,
            },
            {
                "name":
                "multiline_bullet_reflowed",
                "draft":
                """
                    [Comp] Subject
                    - First part
                      second part.
                    - Next bullet.
                """,
                "final":
                """
                    [Comp] Subject

                    - First part second part.
                    - Next bullet.
                """,
            },
        ]

        for test_case in test_cases:
            with self.subTest(test_case["name"]):
                width = test_case.get("width", 72)
                draft = textwrap.dedent(test_case["draft"]).strip()
                final = textwrap.dedent(test_case["final"]).strip()
                self.assertEqual(wrap_text(draft, width=width), final)


if __name__ == '__main__':
    unittest.main()

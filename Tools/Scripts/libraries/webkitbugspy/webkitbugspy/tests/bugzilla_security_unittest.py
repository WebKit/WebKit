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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest
from webkitbugspy import bugzilla


class TestBugzillaSecurityKeywords(unittest.TestCase):
    def test_check_security_keywords_uaf(self):
        matches = bugzilla.Tracker.check_security_keywords('UAF in WebCore', 'This bug causes a use-after-free crash')
        self.assertTrue(len(matches) > 0)

    def test_check_security_keywords_xss(self):
        matches = bugzilla.Tracker.check_security_keywords('XSS vulnerability', 'Cross-site scripting attack possible')
        self.assertTrue(len(matches) > 0)

    def test_check_security_keywords_oob(self):
        matches = bugzilla.Tracker.check_security_keywords('OOB access in array', 'Out-of-bounds read in buffer handling')
        self.assertTrue(len(matches) > 0)

    def test_check_security_keywords_toctou(self):
        matches = bugzilla.Tracker.check_security_keywords('Race condition bug', 'TOCTOU vulnerability in file access')
        self.assertTrue(len(matches) > 0)

    def test_check_security_keywords_none(self):
        matches = bugzilla.Tracker.check_security_keywords('Layout issue', 'Text wrapping is broken in CSS flex containers')
        self.assertEqual(len(matches), 0)

    def test_prompt_security_classification_already_security(self):
        project, component = bugzilla.Tracker.prompt_security_classification(['use-after-free'], 'Security', 'Security')
        self.assertEqual(project, 'Security')
        self.assertEqual(component, 'Security')

    def test_prompt_security_classification_no_matches(self):
        project, component = bugzilla.Tracker.prompt_security_classification([], 'WebKit', 'CSS')
        self.assertEqual(project, 'WebKit')
        self.assertEqual(component, 'CSS')

    def test_prompt_security_classification_user_accepts(self):
        """User chooses 'Yes, use Security' at the prompt."""
        from webkitcorepy import OutputCapture
        from webkitcorepy.mocks import Terminal as MockTerminal
        with MockTerminal.input('Yes'), OutputCapture() as captured:
            project, component = bugzilla.Tracker.prompt_security_classification(
                [r'\bUAF\b'], 'WebKit', 'Text',
            )
        self.assertEqual(project, 'Security')
        self.assertEqual(component, 'Security')
        self.assertIn('security-related keywords', captured.stdout.getvalue())

    def test_prompt_security_classification_user_declines(self):
        """User chooses 'No, keep WebKit' at the prompt."""
        from webkitcorepy import OutputCapture
        from webkitcorepy.mocks import Terminal as MockTerminal
        with MockTerminal.input('No'), OutputCapture() as captured:
            project, component = bugzilla.Tracker.prompt_security_classification(
                [r'\bUAF\b'], 'WebKit', 'Text',
            )
        self.assertEqual(project, 'WebKit')
        self.assertEqual(component, 'Text')
        self.assertIn('security-related keywords', captured.stdout.getvalue())


class TestBugzillaClassifyFromRadar(unittest.TestCase):

    def test_classify_from_radar_no_radar(self):
        project, component, forced = bugzilla.Tracker.classify_from_radar(None, 'WebKit', 'Text')
        self.assertEqual(project, 'WebKit')
        self.assertEqual(component, 'Text')
        self.assertFalse(forced)

    def test_classify_from_radar_non_redacted(self):
        """Non-redacted radar should not force Security."""
        from unittest.mock import MagicMock
        radar_issue = MagicMock()
        radar_issue.redacted = False
        project, component, forced = bugzilla.Tracker.classify_from_radar(radar_issue, 'WebKit', 'Text')
        self.assertEqual(project, 'WebKit')
        self.assertEqual(component, 'Text')
        self.assertFalse(forced)

    def test_classify_from_radar_redacted_forces_security(self):
        """Redacted radar should force Security product/component and print notice."""
        from unittest.mock import MagicMock
        from webkitcorepy import OutputCapture
        radar_issue = MagicMock()
        radar_issue.redacted = True
        radar_issue.link = 'rdar://12345678'
        with OutputCapture() as captured:
            project, component, forced = bugzilla.Tracker.classify_from_radar(radar_issue, 'WebKit', 'Text')
        self.assertEqual(project, 'Security')
        self.assertEqual(component, 'Security')
        self.assertTrue(forced)
        self.assertIn('security-sensitive', captured.stdout.getvalue())
        self.assertIn('Overriding', captured.stdout.getvalue())

    def test_classify_from_radar_redacted_already_security(self):
        """Redacted radar with Security already set should print notice but not override warning."""
        from unittest.mock import MagicMock
        from webkitcorepy import OutputCapture
        radar_issue = MagicMock()
        radar_issue.redacted = True
        radar_issue.link = 'rdar://12345678'
        with OutputCapture() as captured:
            project, component, forced = bugzilla.Tracker.classify_from_radar(radar_issue, 'Security', 'Security')
        self.assertEqual(project, 'Security')
        self.assertEqual(component, 'Security')
        self.assertTrue(forced)
        self.assertIn('security-sensitive', captured.stdout.getvalue())
        self.assertNotIn('Overriding', captured.stdout.getvalue())

    def test_classify_from_radar_redacted_none_values(self):
        """Redacted radar with None project/component should print notice (branch.py path)."""
        from unittest.mock import MagicMock
        from webkitcorepy import OutputCapture
        radar_issue = MagicMock()
        radar_issue.redacted = True
        radar_issue.link = 'rdar://12345678'
        with OutputCapture() as captured:
            project, component, forced = bugzilla.Tracker.classify_from_radar(radar_issue, None, None)
        self.assertEqual(project, 'Security')
        self.assertEqual(component, 'Security')
        self.assertTrue(forced)
        self.assertIn('security-sensitive', captured.stdout.getvalue())
        self.assertNotIn('Overriding', captured.stdout.getvalue())

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

import os
from unittest.mock import patch

from webkitbugspy import Tracker, bugzilla, radar
from webkitbugspy import mocks as bmocks
from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Environment
from webkitcorepy.mocks import Terminal as MockTerminal

from webkitscmpy import mocks, program
from webkitscmpy.program.create_bug import CreateBug


class TestCreateBug(testing.PathTestCase):
    basepath = 'mock/repository'
    BUGZILLA = 'https://bugs.example.com'

    def setUp(self):
        super(TestCreateBug, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        # Create description file for tests
        self.desc_file = os.path.join(self.path, 'description.txt')
        with open(self.desc_file, 'w') as f:
            f.write('Test bug description')

    def test_no_bugzilla_tracker(self):
        """Test error when no bug tracker configured"""
        with OutputCapture() as captured, mocks.local.Git(self.path), \
             patch('webkitbugspy.Tracker._trackers', []):
            self.assertEqual(1, program.main(
                args=('create-bug', '--title', 'Test bug', '-F', self.desc_file),
                path=self.path,
            ))
        self.assertIn('No bug tracker configured', captured.stderr.getvalue())

    def test_description_file_not_found(self):
        """Test error when description file doesn't exist"""
        with OutputCapture() as captured, mocks.local.Git(self.path), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            self.assertEqual(1, program.main(
                args=('create-bug', '--title', 'Test bug', '-F', '/nonexistent/file.txt'),
                path=self.path,
            ))
        self.assertIn('Failed to read description file', captured.stderr.getvalue())

    def test_missing_description(self):
        """Test error when no description file specified"""
        with OutputCapture() as captured, mocks.local.Git(self.path), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            self.assertEqual(1, program.main(
                args=('create-bug', '--title', 'Test bug'),
                path=self.path,
            ))
        self.assertIn('Bug description is required', captured.stderr.getvalue())

    def test_basic_bug_creation(self):
        """Test basic bug creation with title, description file, and component"""
        with OutputCapture() as captured, mocks.local.Git(self.path), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            self.assertEqual(0, program.main(
                args=('create-bug', '--title', 'Test bug', '-F', self.desc_file,
                      '--component', 'Text', '--project', 'WebKit'),
                path=self.path,
            ))
        self.assertIn('Created bug:', captured.stdout.getvalue())

    def test_bug_creation_prompts_for_component(self):
        """Test interactive prompt when component not specified"""
        # First input selects project, second selects component
        with MockTerminal.input('1', '1'), OutputCapture() as captured, mocks.local.Git(self.path), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            self.assertEqual(0, program.main(
                args=('create-bug', '--title', 'Test bug', '-F', self.desc_file),
                path=self.path,
            ))
        self.assertIn('Created bug:', captured.stdout.getvalue())

    def test_radar_arg_accepted(self):
        """Test that --radar is accepted and the bug is created successfully.
        cc_radar is a no-op in this mock environment (no radar_importer configured)."""
        with OutputCapture() as captured, mocks.local.Git(self.path), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            self.assertEqual(0, program.main(
                args=('create-bug', '--title', 'Layout bug', '-F', self.desc_file,
                      '--component', 'Text', '--project', 'WebKit',
                      '--radar', 'rdar://123456789'),
                path=self.path,
            ))
        self.assertIn('Created bug:', captured.stdout.getvalue())

    def test_invalid_radar_arg_errors(self):
        """Test that a malformed --radar value produces an error and no bug is created."""
        with OutputCapture() as captured, mocks.local.Git(self.path), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            self.assertEqual(1, program.main(
                args=('create-bug', '--title', 'Layout bug', '-F', self.desc_file,
                      '--radar', 'not-a-radar'),
                path=self.path,
            ))
        self.assertIn('Invalid --radar value', captured.stderr.getvalue())
        self.assertNotIn('Created bug:', captured.stdout.getvalue())

    def test_radar_importer_not_double_cced(self):
        """Radar importer in --cc should be filtered out; cc_radar() handles it."""
        importer_email = 'webkit-bug-importer@webkit.org'
        with OutputCapture() as captured, mocks.local.Git(self.path), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(
            self.BUGZILLA,
            radar_importer={'name': 'WebKit Bug Importer', 'username': importer_email, 'emails': [importer_email]},
        )]):
            self.assertEqual(0, program.main(
                args=('create-bug', '--title', 'Test bug', '-F', self.desc_file,
                      '--component', 'Text', '--project', 'WebKit',
                      '--cc', 'other@example.com,{}'.format(importer_email)),
                path=self.path,
            ))
        stdout = captured.stdout.getvalue()
        self.assertIn('Created bug:', stdout)
        # The importer should not appear in the explicit CC output — cc_radar() handles it.
        for line in stdout.splitlines():
            if line.startswith('Added CC:'):
                self.assertNotIn(importer_email, line)


class TestCreateBugRadarArgValidation(testing.PathTestCase):
    basepath = 'mock/repository'

    def _valid(self, value):
        """Assert parse_radar_arg succeeds and returns a string."""
        result = CreateBug.parse_radar_arg(value)
        self.assertIsInstance(result, str)
        return result

    def _invalid(self, value):
        """Assert parse_radar_arg raises ValueError."""
        with self.assertRaises(ValueError):
            CreateBug.parse_radar_arg(value)

    # --- Valid formats ---

    def test_bare_id(self):
        self.assertEqual(self._valid('123456789'), 'rdar://123456789')

    def test_rdar_short_url(self):
        self.assertEqual(self._valid('rdar://123456789'), 'rdar://123456789')

    def test_rdar_long_url(self):
        self.assertEqual(self._valid('rdar://problem/123456789'), 'rdar://123456789')

    def test_rdar_short_url_with_angle_brackets(self):
        self.assertEqual(self._valid('<rdar://123456789>'), 'rdar://123456789')

    def test_rdar_long_url_with_angle_brackets(self):
        self.assertEqual(self._valid('<rdar://problem/123456789>'), 'rdar://123456789')

    def test_radar_scheme_short_url(self):
        """radar:// scheme (alternate) is also valid."""
        result = self._valid('radar://123456789')
        self.assertIn('123456789', result)

    # --- Invalid formats (multiple IDs) ---

    def test_space_separated_ids(self):
        self._invalid('123456789 987654321')

    def test_comma_separated_ids(self):
        self._invalid('123456789,987654321')

    def test_comma_and_space_separated_ids(self):
        self._invalid('123456789, 987654321')

    def test_rdar_short_url_ampersand_ids(self):
        self._invalid('rdar://123456789&987654321&111111111')

    def test_rdar_long_url_ampersand_ids(self):
        self._invalid('rdar://problem/123456789&987654321&111111111')

    # --- Invalid formats (unrecognized) ---

    def test_garbage_string(self):
        self._invalid('not-a-radar')

    def test_https_url(self):
        """https://rdar.apple.com/ URLs are now supported by --radar via parse_id."""
        self.assertEqual(self._valid('https://rdar.apple.com/123456789'), 'rdar://123456789')

    def test_empty_string(self):
        self._invalid('')

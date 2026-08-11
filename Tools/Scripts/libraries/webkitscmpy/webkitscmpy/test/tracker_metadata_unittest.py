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

import json
import os
from unittest.mock import patch

from webkitbugspy import Tracker, bugzilla
from webkitbugspy import mocks as bmocks
from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Environment

from webkitscmpy import mocks, program


class TestTrackerMetadata(testing.PathTestCase):
    basepath = 'mock/repository'
    BUGZILLA = 'https://bugs.example.com'

    def setUp(self):
        super(TestTrackerMetadata, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))

    def test_no_tracker(self):
        """Test error when no bug tracker configured"""
        with OutputCapture() as captured, mocks.local.Git(self.path), \
             patch('webkitbugspy.Tracker._trackers', []):
            self.assertEqual(1, program.main(
                args=('tracker-metadata',),
                path=self.path,
            ))
        self.assertIn('No bug tracker configured', captured.stderr.getvalue())

    def test_json_output_all_properties(self):
        """Test JSON output with all default properties"""
        with OutputCapture() as captured, mocks.local.Git(self.path), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            self.assertEqual(0, program.main(
                args=('tracker-metadata',),
                path=self.path,
            ))
        data = json.loads(captured.stdout.getvalue())
        self.assertIn('products', data)
        self.assertIn('components', data)
        self.assertIn('component_descriptions', data)
        self.assertIn('keywords', data)
        self.assertIn('keyword_descriptions', data)
        self.assertEqual(sorted(data['products']), ['CFNetwork', 'WebKit'])
        self.assertIn('WebKit', data['components'])
        self.assertIn('Scrolling', data['components']['WebKit'])
        self.assertIn('InRadar', data['keywords'])

    def test_project_filter(self):
        """Test --project filters components to a single project"""
        with OutputCapture() as captured, mocks.local.Git(self.path), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            self.assertEqual(0, program.main(
                args=('tracker-metadata', '--project', 'WebKit'),
                path=self.path,
            ))
        data = json.loads(captured.stdout.getvalue())
        # Products list is unfiltered
        self.assertEqual(sorted(data['products']), ['CFNetwork', 'WebKit'])
        # Components filtered to WebKit only
        self.assertIn('WebKit', data['components'])
        self.assertNotIn('CFNetwork', data['components'])
        self.assertIn('WebKit', data['component_descriptions'])
        self.assertNotIn('CFNetwork', data['component_descriptions'])

    def test_specific_properties(self):
        """Test selecting specific properties with -p"""
        with OutputCapture() as captured, mocks.local.Git(self.path), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            self.assertEqual(0, program.main(
                args=('tracker-metadata', '-p', 'products', '-p', 'keywords'),
                path=self.path,
            ))
        data = json.loads(captured.stdout.getvalue())
        self.assertIn('products', data)
        self.assertIn('keywords', data)
        self.assertNotIn('components', data)
        self.assertNotIn('component_descriptions', data)
        self.assertNotIn('keyword_descriptions', data)

    def test_text_output(self):
        """Test --format text output"""
        with OutputCapture() as captured, mocks.local.Git(self.path), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            self.assertEqual(0, program.main(
                args=('tracker-metadata', '--format', 'text'),
                path=self.path,
            ))
        output = captured.stdout.getvalue()
        self.assertIn('products:', output)
        self.assertIn('WebKit', output)
        self.assertIn('keywords:', output)

    def test_invalid_property(self):
        """Test error for unknown property"""
        with OutputCapture() as captured, mocks.local.Git(self.path), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            self.assertEqual(1, program.main(
                args=('tracker-metadata', '-p', 'nonexistent'),
                path=self.path,
            ))
        self.assertIn('Unknown property', captured.stderr.getvalue())

    def test_component_descriptions(self):
        """Test that component descriptions are included"""
        with OutputCapture() as captured, mocks.local.Git(self.path), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            self.assertEqual(0, program.main(
                args=('tracker-metadata', '-p', 'component_descriptions', '--project', 'WebKit'),
                path=self.path,
            ))
        data = json.loads(captured.stdout.getvalue())
        descs = data['component_descriptions']['WebKit']
        self.assertEqual(descs['Scrolling'], 'Bugs related to main thread and off-main thread scrolling')
        self.assertEqual(descs['Text'], 'For bugs in text layout and rendering, including international text support.')

    def test_keyword_descriptions(self):
        """Test that keyword descriptions are included"""
        with OutputCapture() as captured, mocks.local.Git(self.path), bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
            environment=Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ),
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            self.assertEqual(0, program.main(
                args=('tracker-metadata', '-p', 'keyword_descriptions'),
                path=self.path,
            ))
        data = json.loads(captured.stdout.getvalue())
        self.assertIn('InRadar', data['keyword_descriptions'])
        self.assertEqual(
            data['keyword_descriptions']['InRadar'],
            'This bug also has a copy in Apple Radar.',
        )

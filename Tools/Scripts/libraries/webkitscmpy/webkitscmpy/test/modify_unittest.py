# Copyright (C) 2025 Apple Inc. All rights reserved.
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
import time
from unittest.mock import patch

from webkitbugspy import Tracker, radar
from webkitbugspy import mocks as bmocks
from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Environment
from webkitcorepy.mocks import Terminal as MockTerminal

from webkitscmpy import mocks, program


class TestModify(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestModify, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_basic(self):
        # Test that modify handles commits without issue references gracefully
        with mocks.local.Git(self.path), mocks.local.Svn(), Environment(RADAR_USERNAME='tcontributor'), bmocks.Radar(
            issues=bmocks.ISSUES,
            projects=bmocks.PROJECTS,
        ) as mock_radar, OutputCapture() as captured, patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):

            result = program.main(
                args=('modify', 'd8bce26f', '--keyword', 'InRadar'),
                path=self.path,
            )

            # Should return 0 even if no commits were processed
            self.assertEqual(0, result)

            # No issue processing occurred because commit doesn't reference an issue
            self.assertIn("doesn't reference any issues", captured.stderr.getvalue())
            self.assertEqual('', captured.stdout.getvalue())

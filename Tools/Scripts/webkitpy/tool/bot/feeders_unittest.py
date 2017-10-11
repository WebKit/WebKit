# Copyright (c) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from datetime import datetime
import unittest

from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.bot.feeders import *
from webkitpy.tool.mocktool import MockTool


class FeedersTest(unittest.TestCase):
    def test_commit_queue_feeder(self):
        self.maxDiff = None
        feeder = CommitQueueFeeder(MockTool())
        expected_logs = """Warning, attachment 10001 on bug 50000 has invalid committer (non-committer@example.com)
Warning, attachment 10001 on bug 50000 has invalid committer (non-committer@example.com)
MOCK setting flag 'commit-queue' to '-' on attachment '10001' with comment 'Rejecting attachment 10001 from commit-queue.\n\nnon-committer@example.com does not have committer permissions according to https://trac.webkit.org/browser/trunk/Tools/Scripts/webkitpy/common/config/contributors.json.

- If you do not have committer rights please read http://webkit.org/coding/contributing.html for instructions on how to use bugzilla flags.

- If you have committer rights please correct the error in Tools/Scripts/webkitpy/common/config/contributors.json by adding yourself to the file (no review needed).  The commit-queue restarts itself every 2 hours.  After restart the commit-queue will correctly respect your committer rights.'
Feeding commit-queue high priority items [10005], regular items [10000]
MOCK: update_work_items: commit-queue [10005, 10000]
"""
        OutputCapture().assert_outputs(self, feeder.feed, expected_logs=expected_logs)

    def _mock_attachment(self, is_rollout, attach_date):
        attachment = Mock()
        attachment.is_rollout = lambda: is_rollout
        attachment.attach_date = lambda: attach_date
        return attachment

    def test_patches_with_acceptable_review_flag(self):
        class MockPatch(object):
            def __init__(self, patch_id, review):
                self.id = patch_id
                self.review = lambda: review

        feeder = CommitQueueFeeder(MockTool())
        patches = [MockPatch(1, None), MockPatch(2, '-'), MockPatch(3, "+")]
        self.assertEqual([patch.id for patch in feeder._patches_with_acceptable_review_flag(patches)], [1, 3])

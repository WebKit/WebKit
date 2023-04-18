# Copyright (C) 2023 Apple Inc. All rights reserved.
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

import logging
import sys

from webkitpy.tool.steps.abstractstep import AbstractStep

_log = logging.getLogger(__name__)


class CheckForRedactedIssue(AbstractStep):
    def run(self, state):
        issues = state.get('issues')
        if issues is None:
            _log.error('Failed to populate issues from commit message')
            sys.exit(1)

        redaction_exemption = None
        redacted_issue = None
        for candidate in issues:
            if getattr(candidate.redacted, 'exemption', False):
                redaction_exemption = candidate
            elif candidate.redacted:
                redacted_issue = candidate
        if redaction_exemption:
            _log.info('The patch you are uploading references {}'.format(redaction_exemption.link))
            _log.info("{} {}".format(redaction_exemption.link, redaction_exemption.redacted))
            if redacted_issue:
                _log.error("Redaction exemption overrides the redaction of {}".format(redacted_issue.link))
                _log.error("{} {}".format(redacted_issue.link, redacted_issue.redacted))
            redacted_issue = None

        if redacted_issue:
            _log.error('The patch you are uploading references {}'.format(redacted_issue.link))
            _log.error("{} {}".format(redacted_issue.link, redacted_issue.redacted))
            _log.error("Please use 'git-webkit' to upload this fix. 'webkit-patch' does not support security changes")
            sys.exit(1)

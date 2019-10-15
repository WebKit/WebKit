# Copyright (C) 2018 Apple Inc. All rights reserved.
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

import unittest

from datetime import datetime
from webkitpy.common.net.bugzilla.constants import BUGZILLA_DATE_FORMAT

from webkitpy.common.net.bugzilla.attachment import Attachment


class AttachmentTest(unittest.TestCase):
    def test_no_bug_id(self):
        self.assertEqual(Attachment({'id': 12345}, None).bug_id(), None)

    def test_convert_to_json_and_back(self):
        bugzilla_formatted_date_string = datetime.today().strftime(BUGZILLA_DATE_FORMAT)
        expected_date = datetime.strptime(bugzilla_formatted_date_string, BUGZILLA_DATE_FORMAT)
        attachment = Attachment({'attach_date': expected_date}, None)
        self.assertEqual(Attachment.from_json(attachment.to_json()).attach_date(), expected_date)

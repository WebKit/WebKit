#!/usr/bin/env python
#
# Copyright (C) 2020 Apple Inc. All rights reserved.
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


import json
import os
import unittest


class EmailsDotJSONTest(unittest.TestCase):
    def test_valid_emails_json(self):
        cwd = os.path.dirname(os.path.abspath(__file__))
        json.load(open(os.path.join(cwd, 'emails.json')))

    def test_emails_json_required_categories_present(self):
        cwd = os.path.dirname(os.path.abspath(__file__))
        emails = json.load(open(os.path.join(cwd, 'emails.json')))
        valid_email_categories = ['ADMIN_EMAILS', 'APPLE_BOT_WATCHERS_EMAILS', 'EMAIL_IDS_TO_UNSUBSCRIBE', 'IGALIA_JSC_TEAM_EMAILS', 'IGALIA_GTK_WPE_EMAILS']
        for category in valid_email_categories:
            self.assertTrue(category in emails.keys())


if __name__ == '__main__':
    unittest.main()

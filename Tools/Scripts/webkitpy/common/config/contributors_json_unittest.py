# Copyright (C) 2022 Apple Inc. All rights reserved.
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
from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.webkit_finder import WebKitFinder


class ContributorsJsonTest(unittest.TestCase):
    def load_json(self):
        json_path = os.path.join(
            WebKitFinder(FileSystem()).webkit_base(),
            'metadata', 'contributors.json',
        )
        try:
            contributors = json.loads(FileSystem().read_text_file(json_path))
        except ValueError as e:
            sys.exit('contributors.json is malformed: ' + str(e))
        return contributors

    def test_valid_contributors_json(self):
        contributors = self.load_json()
        VALID_KEYS = ['name', 'emails', 'github', 'nicks', 'status', 'expertise', 'aliases']
        for contributor in contributors:
            for key in contributor.keys():
                self.assertTrue(key in VALID_KEYS, '{} is not a valid key, valid keys are: {}'.format(key, VALID_KEYS))
            self.assertIsNotNone(contributor.get('name'), contributor)
            self.assertIsNotNone(contributor.get('emails'), contributor)
            self.assertTrue(contributor.get('status') in ['committer', 'reviewer', 'bot', None], '{} is not a valid status'.format(contributor.get('status')))

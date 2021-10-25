# Copyright (C) 2021 Apple Inc. All rights reserved.
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

# Examples of how to run:
# python3 Source/WebKit/Scripts/webkit/model_unittest.py
# cd Source/WebKit/Scripts && python3 -m webkit.model_unittest
# cd Source/WebKit/Scripts && python3 -m unittest discover -p '*_unittest.py'

import os
import sys
import unittest
if sys.version_info > (3, 0):
    from io import StringIO
else:
    from StringIO import StringIO
module_directory = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.abspath(os.path.join(module_directory, os.path.pardir)))
from webkit import model  # noqa: E402
from webkit import parser  # noqa: E402


class ModelCheckTest(unittest.TestCase):

    def test_duplicate_receivers(self):
        contents = """
messages -> WebPage {
    LoadURL(String url)
}"""
        receiver = parser.parse(StringIO(contents))
        self.assertEqual(receiver.name, 'WebPage')
        self.assertEqual(receiver.messages[0].name, 'LoadURL')

        other_contents = """
    messages -> WebPage {
        LoadURL(String url)
        LoadURL2(String url)
    }"""

        other_receiver = parser.parse(StringIO(other_contents))
        self.assertEqual(other_receiver.name, 'WebPage')
        self.assertEqual(other_receiver.messages[0].name, 'LoadURL')
        self.assertEqual(other_receiver.messages[1].name, 'LoadURL2')
        errors = model.check_global_model_inputs([receiver, other_receiver])
        self.assertEqual(len(errors), 1)
        self.assertTrue("Duplicate" in errors[0])

    def test_mismatch_message_attribute_sync(self):
        contents = """
messages -> WebPage {
#if USE(COCOA)
    LoadURL(String url) Synchronous
#endif
#if USE(GTK)
    LoadURL(String url)
#endif
}"""
        receiver = parser.parse(StringIO(contents))
        self.assertEqual(receiver.name, 'WebPage')
        self.assertEqual(receiver.messages[0].name, 'LoadURL')
        errors = model.check_global_model_inputs([receiver])
        self.assertEqual(len(errors), 1)
        self.assertTrue("attribute mismatch" in errors[0])


if __name__ == '__main__':
    unittest.main()

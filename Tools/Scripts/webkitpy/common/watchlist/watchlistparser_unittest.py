# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

'''Unit tests for watchlistparser.py.'''

import re
import unittest
from webkitpy.common.watchlist.watchlistparser import WatchListParser


class WatchListParserTest(unittest.TestCase):
    def setUp(self):
        # For versions of Python before 2.7.
        if not 'assertRaisesRegexp' in dir(self):
            self.assertRaisesRegexp = self._verifyException
        self._watch_list_parser = WatchListParser()

    def _verifyException(self, regex_message, callable, *args):
        try:
            callable(*args)
            self.assertTrue(False, 'No assert raised.')
        except Exception, exception:
            self.assertTrue(re.match(regex_message, exception.__str__()),
                            'Expected regex "%s"\nGot "%s"' % (regex_message, exception.__str__()))

    def test_bad_section(self):
        watch_list_with_bad_section = ('{"FOO": {}}')
        self.assertRaisesRegexp('Unknown section "FOO" in watch list.',
                                self._watch_list_parser.parse, watch_list_with_bad_section)

    def test_section_typo(self):
        watch_list_with_bad_section = ('{"DEFINTIONS": {}}')
        self.assertRaisesRegexp(r'Unknown section "DEFINTIONS" in watch list\.\s*'
                              + r'Perhaps it should be DEFINITIONS\.',
                                self._watch_list_parser.parse, watch_list_with_bad_section)

    def test_bad_definition(self):
        watch_list_with_bad_definition = (
            '{'
            '    "DEFINITIONS": {'
            '        "WatchList1|A": {'
            '            "filename": r".*\\MyFileName\\.cpp",'
            '        },'
            '     },'
            '}')

        self._verifyException(r'Invalid character "\|" in definition "WatchList1\|A"\.',
                              self._watch_list_parser.parse, watch_list_with_bad_definition)

    def test_bad_match_type(self):
        watch_list_with_bad_match_type = (
            '{'
            '    "DEFINITIONS": {'
            '        "WatchList1": {'
            '            "nothing_matches_this": r".*\\MyFileName\\.cpp",'
            '        },'
            '     },'
            '}')

        self._verifyException('Unknown pattern type "nothing_matches_this" in definition "WatchList1".',
                              self._watch_list_parser.parse, watch_list_with_bad_match_type)

    def test_match_type_typo(self):
        watch_list_with_bad_match_type = (
            '{'
            '    "DEFINITIONS": {'
            '        "WatchList1": {'
            '            "iflename": r".*\\MyFileName\\.cpp",'
            '        },'
            '     },'
            '}')

        self._verifyException(r'Unknown pattern type "iflename" in definition "WatchList1"\.\s*'
                              + r'Perhaps it should be filename\.',
                              self._watch_list_parser.parse, watch_list_with_bad_match_type)

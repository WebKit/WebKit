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


import logging
import sys


from webkitpy.common import webkitunittest
from webkitpy.common.watchlist.watchlistparser import WatchListParser

from webkitcorepy import OutputCapture


class WatchListParserTest(webkitunittest.TestCase):
    def setUp(self):
        webkitunittest.TestCase.setUp(self)
        self._watch_list_parser = WatchListParser()

    def test_bad_section(self):
        watch_list = ('{"FOO": {}}')
        with OutputCapture(level=logging.INFO) as captured:
            self._watch_list_parser.parse(watch_list)
        self.assertEqual(
            captured.root.log.getvalue(),
            'Unknown section "FOO" in watch list.\n',
        )

    def test_section_typo(self):
        watch_list = ('{"DEFINTIONS": {}}')
        with OutputCapture(level=logging.INFO) as captured:
            self._watch_list_parser.parse(watch_list)
        self.assertEqual(
            captured.root.log.getvalue(),
            'Unknown section "DEFINTIONS" in watch list.\n\nPerhaps it should be DEFINITIONS.\n',
        )

    def test_bad_definition(self):
        watch_list = (
            '{'
            '    "DEFINITIONS": {'
            '        "WatchList1|A": {'
            '            "filename": r".*MyFileName\\.cpp",'
            '        },'
            '     },'
            '}')

        with OutputCapture(level=logging.INFO) as captured:
            self._watch_list_parser.parse(watch_list)
        self.assertEqual(captured.root.log.getvalue(), 'Invalid character "|" in definition "WatchList1|A".\n')

    def test_bad_filename_regex(self):
        watch_list = (
            '{'
            '    "DEFINITIONS": {'
            '        "WatchList1": {'
            '            "filename": r"*",'
            '            "more": r"RefCounted",'
            '        },'
            '     },'
            '    "CC_RULES": {'
            '        "WatchList1": ["levin@chromium.org"],'
            '     },'
            '}')

        with OutputCapture(level=logging.INFO) as captured:
            self._watch_list_parser.parse(watch_list)
        if sys.version_info > (3, 0):
            expected_log = 'The regex "*" is invalid due to "nothing to repeat at position 0".\n'
        else:
            expected_log = 'The regex "*" is invalid due to "nothing to repeat".\n'
        self.assertEqual(captured.root.log.getvalue(), expected_log)

    def test_bad_more_regex(self):
        watch_list = (
            '{'
            '    "DEFINITIONS": {'
            '        "WatchList1": {'
            '            "filename": r"aFileName\\.cpp",'
            '            "more": r"*",'
            '        },'
            '     },'
            '    "CC_RULES": {'
            '        "WatchList1": ["levin@chromium.org"],'
            '     },'
            '}')

        with OutputCapture(level=logging.INFO) as captured:
            self._watch_list_parser.parse(watch_list)
        if sys.version_info > (3, 0):
            expected_log = 'The regex "*" is invalid due to "nothing to repeat at position 0".\n'
        else:
            expected_log = 'The regex "*" is invalid due to "nothing to repeat".\n'
        self.assertEqual(captured.root.log.getvalue(), expected_log)

    def test_bad_match_type(self):
        watch_list = (
            '{'
            '    "DEFINITIONS": {'
            '        "WatchList1": {'
            '            "nothing_matches_this": r".*MyFileName\\.cpp",'
            '            "filename": r".*MyFileName\\.cpp",'
            '        },'
            '     },'
            '    "CC_RULES": {'
            '        "WatchList1": ["levin@chromium.org"],'
            '     },'
            '}')

        with OutputCapture(level=logging.INFO) as captured:
            self._watch_list_parser.parse(watch_list)
        self.assertEqual(
            captured.root.log.getvalue(),
            'Unknown pattern type "nothing_matches_this" in definition "WatchList1".\n',
        )

    def test_match_type_typo(self):
        watch_list = (
            '{'
            '    "DEFINITIONS": {'
            '        "WatchList1": {'
            '            "iflename": r".*MyFileName\\.cpp",'
            '            "more": r"RefCounted",'
            '        },'
            '     },'
            '    "CC_RULES": {'
            '        "WatchList1": ["levin@chromium.org"],'
            '     },'
            '}')

        with OutputCapture(level=logging.INFO) as captured:
            self._watch_list_parser.parse(watch_list)
        self.assertEqual(
            captured.root.log.getvalue(),
            'Unknown pattern type "iflename" in definition "WatchList1".\n\nPerhaps it should be filename.\n',
        )

    def test_empty_definition(self):
        watch_list = (
            '{'
            '    "DEFINITIONS": {'
            '        "WatchList1": {'
            '        },'
            '     },'
            '    "CC_RULES": {'
            '        "WatchList1": ["levin@chromium.org"],'
            '     },'
            '}')

        with OutputCapture(level=logging.INFO) as captured:
            self._watch_list_parser.parse(watch_list)
        self.assertEqual(
            captured.root.log.getvalue(),
            'The definition "WatchList1" has no patterns, so it should be deleted.\n',
        )

    def test_empty_cc_rule(self):
        watch_list = (
            '{'
            '    "DEFINITIONS": {'
            '        "WatchList1": {'
            '            "filename": r".*MyFileName\\.cpp",'
            '        },'
            '     },'
            '    "CC_RULES": {'
            '        "WatchList1": [],'
            '     },'
            '}')

        with OutputCapture(level=logging.INFO) as captured:
            self._watch_list_parser.parse(watch_list)
        self.assertEqual(
            captured.root.log.getvalue(),
            'A rule for definition "WatchList1" is empty, so it should be deleted.\nThe following definitions are not '
            'used and should be removed: WatchList1\n',
        )

    def test_cc_rule_with_invalid_email(self):
        watch_list = (
            '{'
            '    "DEFINITIONS": {'
            '        "WatchList1": {'
            '            "filename": r".*MyFileName\\.cpp",'
            '        },'
            '     },'
            '    "CC_RULES": {'
            '        "WatchList1": ["levin+bad+email@chromium.org"],'
            '     },'
            '}')

        with OutputCapture(level=logging.INFO) as captured:
            self._watch_list_parser.parse(watch_list)
        self.assertEqual(
            captured.root.log.getvalue(),
            'The email alias levin+bad+email@chromium.org which is in the watchlist is not listed as a contributor in '
            'contributors.json\n',
        )

    def test_cc_rule_with_secondary_email(self):
        # FIXME: We should provide a mock of CommitterList so that we can test this on fake data.
        watch_list = (
            '{'
            '    "DEFINITIONS": {'
            '        "WatchList1": {'
            '            "filename": r".*MyFileName\\.cpp",'
            '        },'
            '     },'
            '    "CC_RULES": {'
            '        "WatchList1": ["ojan.autocc@gmail.com"],'
            '     },'
            '}')

        with OutputCapture(level=logging.INFO) as captured:
            self._watch_list_parser.parse(watch_list)
        self.assertEqual(captured.root.log.getvalue(), '')

    def test_empty_message_rule(self):
        watch_list = (
            '{'
            '    "DEFINITIONS": {'
            '        "WatchList1": {'
            '            "filename": r".*MyFileName\\.cpp",'
            '        },'
            '     },'
            '    "MESSAGE_RULES": {'
            '        "WatchList1": ['
            '        ],'
            '     },'
            '}')

        with OutputCapture(level=logging.INFO) as captured:
            self._watch_list_parser.parse(watch_list)
        self.assertEqual(
            captured.root.log.getvalue(),
            'A rule for definition "WatchList1" is empty, so it should be deleted.\nThe following definitions are not '
            'used and should be removed: WatchList1\n',
        )

    def test_unused_defintion(self):
        watch_list = (
            '{'
            '    "DEFINITIONS": {'
            '        "WatchList1": {'
            '            "filename": r".*MyFileName\\.cpp",'
            '        },'
            '     },'
            '}')

        with OutputCapture(level=logging.INFO) as captured:
            self._watch_list_parser.parse(watch_list)
        self.assertEqual(
            captured.root.log.getvalue(),
            'The following definitions are not used and should be removed: WatchList1\n',
        )

    def test_cc_rule_with_undefined_defintion(self):
        watch_list = (
            '{'
            '    "CC_RULES": {'
            '        "WatchList1": ["levin@chromium.org"]'
            '     },'
            '}')

        with OutputCapture(level=logging.INFO) as captured:
            self._watch_list_parser.parse(watch_list)
        self.assertEqual(
            captured.root.log.getvalue(),
            'In section "CC_RULES", the following definitions are not used and should be removed: WatchList1\n',
        )

    def test_message_rule_with_undefined_defintion(self):
        watch_list = (
            '{'
            '    "MESSAGE_RULES": {'
            '        "WatchList1": ["The message."]'
            '     },'
            '}')

        with OutputCapture(level=logging.INFO) as captured:
            self._watch_list_parser.parse(watch_list)
        self.assertEqual(
            captured.root.log.getvalue(),
            'In section "MESSAGE_RULES", the following definitions are not used and should be removed: WatchList1\n',
        )

    def test_cc_rule_with_undefined_defintion_with_suggestion(self):
        watch_list = (
            '{'
            '    "DEFINITIONS": {'
            '        "WatchList1": {'
            '            "filename": r".*MyFileName\\.cpp",'
            '        },'
            '     },'
            '    "CC_RULES": {'
            '        "WatchList": ["levin@chromium.org"]'
            '     },'
            '    "MESSAGE_RULES": {'
            '        "WatchList1": ["levin@chromium.org"]'
            '     },'
            '}')

        with OutputCapture(level=logging.INFO) as captured:
            self._watch_list_parser.parse(watch_list)
        self.assertEqual(
            captured.root.log.getvalue(),
            'In section "CC_RULES", the following definitions are not used and should be removed: '
            'WatchList\n\nPerhaps it should be WatchList1.\n',
        )

    def test_cc_rule_with_complex_logic(self):
        watch_list = (
            '{'
            '    "DEFINITIONS": {'
            '        "WatchList1": {'
            '            "filename": r".*MyFileName\\.cpp",'
            '        },'
            '        "WatchList2": {'
            '            "filename": r".*MyFileName\\.h",'
            '        },'
            '        "WatchList3": {'
            '            "filename": r".*MyFileName\\.o",'
            '        },'
            '     },'
            '    "CC_RULES": {'
            '        "!WatchList1&!WatchList2|!WatchList3&!WatchListUndefined": ["clopez@igalia.com"]'
            '     },'
            '    "MESSAGE_RULES": {'
            '        "!WatchList1|WatchList2&!WatchList3|!WatchListUndefined": ["clopez@igalia.com"]'
            '     },'
            '}')

        with OutputCapture(level=logging.INFO) as captured:
            self._watch_list_parser.parse(watch_list)
        self.assertEqual(
            captured.root.log.getvalue(),
            '''In section "CC_RULES", the following definitions are not used and should be removed: WatchListUndefined

Perhaps it should be WatchList3 or WatchList2 or WatchList1.
In section "MESSAGE_RULES", the following definitions are not used and should be removed: WatchListUndefined

Perhaps it should be WatchList3 or WatchList2 or WatchList1.
''',
        )

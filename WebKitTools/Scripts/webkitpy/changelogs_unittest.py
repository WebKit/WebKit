# Copyright (C) 2009 Google Inc. All rights reserved.
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

import unittest
from changelogs import *

import os
import tempfile
from StringIO import StringIO

class ChangeLogsTest(unittest.TestCase):

    _example_entry = '''2009-08-17  Peter Kasting  <pkasting@google.com>

        Reviewed by Steve Falkenburg.

        https://bugs.webkit.org/show_bug.cgi?id=27323
        Only add Cygwin to the path when it isn't already there.  This avoids
        causing problems for people who purposefully have non-Cygwin versions of
        executables like svn in front of the Cygwin ones in their paths.

        * DumpRenderTree/win/DumpRenderTree.vcproj:
        * DumpRenderTree/win/ImageDiff.vcproj:
        * DumpRenderTree/win/TestNetscapePlugin/TestNetscapePlugin.vcproj:
'''

    # More example text than we need.  Eventually we need to support parsing this all and write tests for the parsing.
    _example_changelog = '''2009-08-17  David Kilzer  <ddkilzer@apple.com>

        <http://webkit.org/b/28393> check-webkit-style: add check for use of std::max()/std::min() instead of MAX()/MIN()

        Reviewed by David Levin.

        * Scripts/modules/cpp_style.py:
        (_ERROR_CATEGORIES): Added 'runtime/max_min_macros'.
        (check_max_min_macros): Added.  Returns level 4 error when MAX()
        and MIN() macros are used in header files and C++ source files.
        (check_style): Added call to check_max_min_macros().
        * Scripts/modules/cpp_style_unittest.py: Added unit tests.
        (test_max_macro): Added.
        (test_min_macro): Added.

2009-08-16  David Kilzer  <ddkilzer@apple.com>

        Backed out r47343 which was mistakenly committed

        * Scripts/bugzilla-tool:
        * Scripts/modules/scm.py:

2009-06-18  Darin Adler  <darin@apple.com>

        Rubber stamped by Mark Rowe.

        * DumpRenderTree/mac/DumpRenderTreeWindow.mm:
        (-[DumpRenderTreeWindow close]): Resolved crashes seen during regression
        tests. The close method can be called on a window that's already closed
        so we can't assert here.

== Rolled over to ChangeLog-2009-06-16 ==
'''

    def test_latest_entry_parse(self):
        changelog_contents = "%s\n%s" % (self._example_entry, self._example_changelog)
        changelog_file = StringIO(changelog_contents)
        latest_entry = ChangeLog._parse_latest_entry_from_file(changelog_file)
        self.assertEquals(self._example_entry, latest_entry)

    @staticmethod
    def _write_tmp_file_with_contents(contents):
        (file_descriptor, file_path) = tempfile.mkstemp() # NamedTemporaryFile always deletes the file on close in python < 2.6
        file = os.fdopen(file_descriptor, 'w')
        file.write(contents)
        file.close()
        return file_path

    @staticmethod
    def _read_file_contents(file_path):
        file = open(file_path)
        contents = file.read()
        file.close()
        return contents

    _new_entry_boilerplate = '''2009-08-19  Eric Seidel  <eric@webkit.org>

        Reviewed by NOBODY (OOPS!).

        Need a short description and bug URL (OOPS!)

        * Scripts/bugzilla-tool:
'''

    def test_set_reviewer(self):
        changelog_contents = "%s\n%s" % (self._new_entry_boilerplate, self._example_changelog)
        changelog_path = self._write_tmp_file_with_contents(changelog_contents)
        reviewer_name = 'Test Reviewer'
        ChangeLog(changelog_path).set_reviewer(reviewer_name)
        actual_contents = self._read_file_contents(changelog_path)
        expected_contents = changelog_contents.replace('NOBODY (OOPS!)', reviewer_name)
        os.remove(changelog_path)
        self.assertEquals(actual_contents, expected_contents)

    _revert_message = """        No review, rolling out r12345.
        http://trac.webkit.org/changeset/12345
        http://example.com/123

        This is a very long reason which should be long enough so that
        _message_for_revert will need to wrap it.  We'll also include
        a
        https://veryveryveryveryverylongbugurl.com/reallylongbugthingy.cgi?bug_id=12354
        link so that we can make sure we wrap that right too.
"""

    def test_message_for_revert(self):
        changelog = ChangeLog("/fake/path")
        long_reason = "This is a very long reason which should be long enough so that _message_for_revert will need to wrap it.  We'll also include a https://veryveryveryveryverylongbugurl.com/reallylongbugthingy.cgi?bug_id=12354 link so that we can make sure we wrap that right too."
        message = changelog._message_for_revert(12345, long_reason, "http://example.com/123")
        self.assertEquals(message, self._revert_message)

    _revert_entry_with_bug_url = '''2009-08-19  Eric Seidel  <eric@webkit.org>

        No review, rolling out r12345.
        http://trac.webkit.org/changeset/12345
        http://example.com/123

        Reason

        * Scripts/bugzilla-tool:
'''

    _revert_entry_without_bug_url = '''2009-08-19  Eric Seidel  <eric@webkit.org>

        No review, rolling out r12345.
        http://trac.webkit.org/changeset/12345

        Reason

        * Scripts/bugzilla-tool:
'''

    def _assert_update_for_revert_output(self, args, expected_entry):
        changelog_contents = "%s\n%s" % (self._new_entry_boilerplate, self._example_changelog)
        changelog_path = self._write_tmp_file_with_contents(changelog_contents)
        changelog = ChangeLog(changelog_path)
        changelog.update_for_revert(*args)
        actual_entry = changelog.latest_entry()
        os.remove(changelog_path)
        self.assertEquals(actual_entry, expected_entry)

    def test_update_for_revert(self):
        self._assert_update_for_revert_output([12345, "Reason"], self._revert_entry_without_bug_url)
        self._assert_update_for_revert_output([12345, "Reason", "http://example.com/123"], self._revert_entry_with_bug_url)

if __name__ == '__main__':
    unittest.main()

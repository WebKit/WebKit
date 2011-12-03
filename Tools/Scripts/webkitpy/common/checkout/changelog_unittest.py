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

from __future__ import with_statement

import codecs
import os
import tempfile
import unittest

from StringIO import StringIO

from webkitpy.common.checkout.changelog import *


class ChangeLogTest(unittest.TestCase):

    _example_entry = u'''2009-08-17  Peter Kasting  <pkasting@google.com>

        Reviewed by Tor Arne Vestb\xf8.

        https://bugs.webkit.org/show_bug.cgi?id=27323
        Only add Cygwin to the path when it isn't already there.  This avoids
        causing problems for people who purposefully have non-Cygwin versions of
        executables like svn in front of the Cygwin ones in their paths.

        * DumpRenderTree/win/DumpRenderTree.vcproj:
        * DumpRenderTree/win/ImageDiff.vcproj:
        * DumpRenderTree/win/TestNetscapePlugin/TestNetscapePlugin.vcproj:
'''

    _rolled_over_footer = '== Rolled over to ChangeLog-2009-06-16 =='

    # More example text than we need.  Eventually we need to support parsing this all and write tests for the parsing.
    _example_changelog = u"""2009-08-17  Tor Arne Vestb\xf8  <vestbo@webkit.org>

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

2011-11-04  Benjamin Poulain  <bpoulain@apple.com>

        [Mac] ResourceRequest's nsURLRequest() does not differentiate null and empty URLs with CFNetwork
        https://bugs.webkit.org/show_bug.cgi?id=71539

        Reviewed by David Kilzer.

        In order to have CFURL and NSURL to be consistent when both are used on Mac,
        KURL::createCFURL() is changed to support empty URL values.

        * This change log entry is made up to test _parse_entry:
            * a list of things

        * platform/cf/KURLCFNet.cpp:
        (WebCore::createCFURLFromBuffer):
        (WebCore::KURL::createCFURL):
        * platform/mac/KURLMac.mm :
        (WebCore::KURL::operator NSURL *):
        (WebCore::KURL::createCFURL):
        * WebCoreSupport/ChromeClientEfl.cpp:
        (WebCore::ChromeClientEfl::closeWindowSoon): call new function and moves its
        previous functionality there.
        * ewk/ewk_private.h:
        * ewk/ewk_view.cpp:

2011-03-02  Carol Szabo  <carol.szabo@nokia.com>

        Reviewed by David Hyatt  <hyatt@apple.com>

        content property doesn't support quotes
        https://bugs.webkit.org/show_bug.cgi?id=6503

        Added full support for quotes as defined by CSS 2.1.

        Tests: fast/css/content/content-quotes-01.html
               fast/css/content/content-quotes-02.html
               fast/css/content/content-quotes-03.html
               fast/css/content/content-quotes-04.html
               fast/css/content/content-quotes-05.html
               fast/css/content/content-quotes-06.html

2011-03-31  Brent Fulgham  <bfulgham@webkit.org>

       Reviewed Adam Roben.

       [WinCairo] Implement Missing drawWindowsBitmap method.
       https://bugs.webkit.org/show_bug.cgi?id=57409

2011-03-28  Dirk Pranke  <dpranke@chromium.org>

       RS=Tony Chang.

       r81977 moved FontPlatformData.h from
       WebCore/platform/graphics/cocoa to platform/graphics. This
       change updates the chromium build accordingly.

       https://bugs.webkit.org/show_bug.cgi?id=57281

       * platform/graphics/chromium/CrossProcessFontLoading.mm:

2011-05-04  Alexis Menard  <alexis.menard@openbossa.org>

       Unreviewed warning fix.

       The variable is just used in the ASSERT macro. Let's use ASSERT_UNUSED to avoid
       a warning in Release build.

       * accessibility/AccessibilityRenderObject.cpp:
       (WebCore::lastChildConsideringContinuation):

2011-10-11  Antti Koivisto  <antti@apple.com>

       Resolve regular and visited link style in a single pass
       https://bugs.webkit.org/show_bug.cgi?id=69838

       Reviewed by Darin Adler

       We can simplify and speed up selector matching by removing the recursive matching done
       to generate the style for the :visited pseudo selector. Both regular and visited link style
       can be generated in a single pass through the style selector.

== Rolled over to ChangeLog-2009-06-16 ==
"""

    def test_parse_bug_id_from_changelog(self):
        commit_text = '''
2011-03-23  Ojan Vafai  <ojan@chromium.org>

        Add failing result for WebKit2. All tests that require
        focus fail on WebKit2. See https://bugs.webkit.org/show_bug.cgi?id=56988.

        * platform/mac-wk2/fast/css/pseudo-any-expected.txt: Added.

        '''

        self.assertEquals(56988, parse_bug_id_from_changelog(commit_text))

        commit_text = '''
2011-03-23  Ojan Vafai  <ojan@chromium.org>

        Add failing result for WebKit2. All tests that require
        focus fail on WebKit2. See https://bugs.webkit.org/show_bug.cgi?id=56988.
        https://bugs.webkit.org/show_bug.cgi?id=12345

        * platform/mac-wk2/fast/css/pseudo-any-expected.txt: Added.

        '''

        self.assertEquals(12345, parse_bug_id_from_changelog(commit_text))

        commit_text = '''
2011-03-31  Adam Roben  <aroben@apple.com>

        Quote the executable path we pass to ::CreateProcessW

        This will ensure that spaces in the path will be interpreted correctly.

        Fixes <http://webkit.org/b/57569> Web process sometimes fails to launch when there are
        spaces in its path

        Reviewed by Steve Falkenburg.

        * UIProcess/Launcher/win/ProcessLauncherWin.cpp:
        (WebKit::ProcessLauncher::launchProcess): Surround the executable path in quotes.

        '''

        self.assertEquals(57569, parse_bug_id_from_changelog(commit_text))

        commit_text = '''
2011-03-29  Timothy Hatcher  <timothy@apple.com>

        Update WebCore Localizable.strings to contain WebCore, WebKit/mac and WebKit2 strings.

        https://webkit.org/b/57354

        Reviewed by Sam Weinig.

        * English.lproj/Localizable.strings: Updated.
        * StringsNotToBeLocalized.txt: Removed. To hard to maintain in WebCore.
        * platform/network/cf/LoaderRunLoopCF.h: Remove a single quote in an #error so
        extract-localizable-strings does not complain about unbalanced single quotes.
        '''

        self.assertEquals(57354, parse_bug_id_from_changelog(commit_text))

    def test_parse_log_entries_from_changelog(self):
        changelog_file = StringIO(self._example_changelog)
        parsed_entries = list(ChangeLog.parse_entries_from_file(changelog_file))
        self.assertEquals(len(parsed_entries), 9)
        self.assertEquals(parsed_entries[0].reviewer_text(), "David Levin")
        self.assertEquals(parsed_entries[1].author_email(), "ddkilzer@apple.com")
        self.assertEquals(parsed_entries[2].reviewer_text(), "Mark Rowe")
        self.assertEquals(parsed_entries[2].touched_files(), ["DumpRenderTree/mac/DumpRenderTreeWindow.mm"])
        self.assertEquals(parsed_entries[3].author_name(), "Benjamin Poulain")
        self.assertEquals(parsed_entries[3].touched_files(), ["platform/cf/KURLCFNet.cpp", "platform/mac/KURLMac.mm",
            "WebCoreSupport/ChromeClientEfl.cpp", "ewk/ewk_private.h", "ewk/ewk_view.cpp"])
        self.assertEquals(parsed_entries[4].reviewer_text(), "David Hyatt")
        self.assertEquals(parsed_entries[5].reviewer_text(), "Adam Roben")
        self.assertEquals(parsed_entries[6].reviewer_text(), "Tony Chang")
        self.assertEquals(parsed_entries[7].reviewer_text(), None)
        self.assertEquals(parsed_entries[8].reviewer_text(), 'Darin Adler')

    def test_parse_log_entries_from_annotated_file(self):
        changelog_file = StringIO(u'''100000 ossy@webkit.org 2011-11-11  Csaba Osztrogon\u00e1c  <ossy@webkit.org>
100000 ossy@webkit.org 
100000 ossy@webkit.org         100,000 !!!
100000 ossy@webkit.org 
100000 ossy@webkit.org         Reviewed by Zoltan Herczeg.
100000 ossy@webkit.org 
100000 ossy@webkit.org         * ChangeLog: Point out revision 100,000.
100000 ossy@webkit.org
93798 ap@apple.com 2011-08-25  Alexey Proskuryakov  <ap@apple.com>
93798 ap@apple.com 
93798 ap@apple.com         Fix build when GCC 4.2 is not installed.
93798 ap@apple.com 
93798 ap@apple.com         * gtest/xcode/Config/CompilerVersion.xcconfig: Copied from Source/WebCore/Configurations/CompilerVersion.xcconfig.
93798 ap@apple.com         * gtest/xcode/Config/General.xcconfig:
93798 ap@apple.com         Use the same compiler version as other projects do.
93798 ap@apple.com
99491 andreas.kling@nokia.com 2011-11-03  Andreas Kling  <kling@webkit.org>
99491 andreas.kling@nokia.com 
99190 andreas.kling@nokia.com         Unreviewed build fix, sigh.
99190 andreas.kling@nokia.com 
99190 andreas.kling@nokia.com         * css/CSSFontFaceRule.h:
99190 andreas.kling@nokia.com         * css/CSSMutableStyleDeclaration.h:
99190 andreas.kling@nokia.com 
99190 andreas.kling@nokia.com 2011-11-03  Andreas Kling  <kling@webkit.org>
99190 andreas.kling@nokia.com 
99187 andreas.kling@nokia.com         Unreviewed build fix, out-of-line StyleSheet::parentStyleSheet()
99187 andreas.kling@nokia.com         again since there's a cycle in the includes between CSSRule/StyleSheet.
99187 andreas.kling@nokia.com 
99187 andreas.kling@nokia.com         * css/StyleSheet.cpp:
99187 andreas.kling@nokia.com         (WebCore::StyleSheet::parentStyleSheet):
99187 andreas.kling@nokia.com         * css/StyleSheet.h:
99187 andreas.kling@nokia.com 
''')
        parsed_entries = list(ChangeLog.parse_entries_from_file(changelog_file))
        self.assertEquals(parsed_entries[0].revision(), 100000)
        self.assertEquals(parsed_entries[0].reviewer_text(), "Zoltan Herczeg")
        self.assertEquals(parsed_entries[0].author_name(), u"Csaba Osztrogon\u00e1c")
        self.assertEquals(parsed_entries[0].author_email(), "ossy@webkit.org")
        self.assertEquals(parsed_entries[1].revision(), 93798)
        self.assertEquals(parsed_entries[1].author_name(), "Alexey Proskuryakov")
        self.assertEquals(parsed_entries[2].revision(), 99190)
        self.assertEquals(parsed_entries[2].author_name(), "Andreas Kling")
        self.assertEquals(parsed_entries[3].revision(), 99187)
        self.assertEquals(parsed_entries[3].author_name(), "Andreas Kling")

    def _assert_parse_reviewer_text_and_list(self, text, expected_reviewer_text, expected_reviewer_text_list=None):
        reviewer_text, reviewer_text_list = ChangeLogEntry._parse_reviewer_text(text)
        self.assertEquals(reviewer_text, expected_reviewer_text)
        if expected_reviewer_text_list:
            self.assertEquals(reviewer_text_list, expected_reviewer_text_list)
        else:
            self.assertEquals(reviewer_text_list, [expected_reviewer_text])

    def _assert_parse_reviewer_text_list(self, text, expected_reviewer_text_list):
        reviewer_text, reviewer_text_list = ChangeLogEntry._parse_reviewer_text(text)
        self.assertEquals(reviewer_text_list, expected_reviewer_text_list)

    def test_parse_reviewer_text(self):
        self._assert_parse_reviewer_text_and_list('  reviewed  by Ryosuke Niwa,   Oliver Hunt, and  Dimitri Glazkov',
            'Ryosuke Niwa, Oliver Hunt, and Dimitri Glazkov', ['Ryosuke Niwa', 'Oliver Hunt', 'Dimitri Glazkov'])
        self._assert_parse_reviewer_text_and_list('Reviewed by Brady Eidson and David Levin, landed by Brady Eidson',
            'Brady Eidson and David Levin', ['Brady Eidson', 'David Levin'])

        self._assert_parse_reviewer_text_and_list('Reviewed by Simon Fraser. Committed by Beth Dakin.', 'Simon Fraser')
        self._assert_parse_reviewer_text_and_list('Reviewed by Geoff Garen. V8 fixes courtesy of Dmitry Titov.', 'Geoff Garen')
        self._assert_parse_reviewer_text_and_list('Reviewed by Adam Roben&Dirk Schulze', 'Adam Roben&Dirk Schulze', ['Adam Roben', 'Dirk Schulze'])
        self._assert_parse_reviewer_text_and_list('Rubber stamps by Darin Adler & Sam Weinig.', 'Darin Adler & Sam Weinig', ['Darin Adler', 'Sam Weinig'])

        self._assert_parse_reviewer_text_and_list('Reviewed by adam,andy and andy adam, andy smith',
            'adam,andy and andy adam, andy smith', ['adam', 'andy', 'andy adam', 'andy smith'])

        self._assert_parse_reviewer_text_and_list('rubber stamped by Oliver Hunt (oliver@apple.com) and Darin Adler (darin@apple.com)',
            'Oliver Hunt and Darin Adler', ['Oliver Hunt', 'Darin Adler'])

        self._assert_parse_reviewer_text_and_list('rubber  Stamped by David Hyatt  <hyatt@apple.com>', 'David Hyatt')
        self._assert_parse_reviewer_text_and_list('Rubber-stamped by Antti Koivisto.', 'Antti Koivisto')
        self._assert_parse_reviewer_text_and_list('Rubberstamped by Dan Bernstein.', 'Dan Bernstein')
        self._assert_parse_reviewer_text_and_list('Reviews by Ryosuke Niwa', 'Ryosuke Niwa')
        self._assert_parse_reviewer_text_and_list('Reviews Ryosuke Niwa', 'Ryosuke Niwa')
        self._assert_parse_reviewer_text_and_list('Rubberstamp Ryosuke Niwa', 'Ryosuke Niwa')
        self._assert_parse_reviewer_text_and_list('Typed and reviewed by Alexey Proskuryakov.', 'Alexey Proskuryakov')
        self._assert_parse_reviewer_text_and_list('Reviewed and landed by Brady Eidson', 'Brady Eidson')
        self._assert_parse_reviewer_text_and_list('Reviewed by rniwa@webkit.org.', 'rniwa@webkit.org')
        self._assert_parse_reviewer_text_and_list('Reviewed by Dirk Schulze / Darin Adler.', 'Dirk Schulze / Darin Adler', ['Dirk Schulze', 'Darin Adler'])
        self._assert_parse_reviewer_text_and_list('Reviewed by Sam Weinig + Oliver Hunt.', 'Sam Weinig + Oliver Hunt', ['Sam Weinig', 'Oliver Hunt'])

        self._assert_parse_reviewer_text_list('Reviewed by Sam Weinig, and given a good once-over by Jeff Miller.', ['Sam Weinig', 'Jeff Miller'])
        self._assert_parse_reviewer_text_list(' Reviewed by Sam Weinig, even though this is just a...', ['Sam Weinig'])
        self._assert_parse_reviewer_text_list('Rubber stamped by by Gustavo Noronha Silva', ['Gustavo Noronha Silva'])
        self._assert_parse_reviewer_text_list('Rubberstamped by Noam Rosenthal, who wrote the original code.', ['Noam Rosenthal'])
        self._assert_parse_reviewer_text_list('Reviewed by Dan Bernstein (relanding of r47157)', ['Dan Bernstein'])
        self._assert_parse_reviewer_text_list('Reviewed by Geoffrey "Sean/Shawn/Shaun" Garen', ['Geoffrey Garen'])
        self._assert_parse_reviewer_text_list('Reviewed by Dave "Messy" Hyatt.', ['Dave Hyatt'])
        self._assert_parse_reviewer_text_list('Reviewed by Sam \'The Belly\' Weinig', ['Sam Weinig'])
        self._assert_parse_reviewer_text_list('Rubber-stamped by David "I\'d prefer not" Hyatt.', ['David Hyatt'])
        self._assert_parse_reviewer_text_list('Reviewed by Mr. Geoffrey Garen.', ['Geoffrey Garen'])
        self._assert_parse_reviewer_text_list('Reviewed by Darin (ages ago)', ['Darin'])
        self._assert_parse_reviewer_text_list('Reviewed by Sam Weinig (except for a few comment and header tweaks).', ['Sam Weinig'])
        self._assert_parse_reviewer_text_list('Reviewed by Sam Weinig (all but the FormDataListItem rename)', ['Sam Weinig'])
        self._assert_parse_reviewer_text_list('Reviewed by Darin Adler, tweaked and landed by Beth.', ['Darin Adler'])
        self._assert_parse_reviewer_text_list('Reviewed by Sam Weinig with no hesitation', ['Sam Weinig'])
        self._assert_parse_reviewer_text_list('Reviewed by Oliver Hunt, okayed by Darin Adler.', ['Oliver Hunt'])
        self._assert_parse_reviewer_text_list('Reviewed by Darin Adler).', ['Darin Adler'])

        # For now, we let unofficial reviewers recognized as reviewers
        self._assert_parse_reviewer_text_list('Reviewed by Sam Weinig, Anders Carlsson, and (unofficially) Adam Barth.',
            ['Sam Weinig', 'Anders Carlsson', 'Adam Barth'])

        self._assert_parse_reviewer_text_list('Reviewed by NOBODY.', None)
        self._assert_parse_reviewer_text_list('Reviewed by NOBODY - Build Fix.', None)
        self._assert_parse_reviewer_text_list('Reviewed by NOBODY, layout tests fix.', None)
        self._assert_parse_reviewer_text_list('Reviewed by NOBODY (Qt build fix pt 2).', None)
        self._assert_parse_reviewer_text_list('Reviewed by NOBODY(rollout)', None)
        self._assert_parse_reviewer_text_list('Reviewed by NOBODY (Build fix, forgot to svn add this file)', None)
        self._assert_parse_reviewer_text_list('Reviewed by nobody (trivial follow up fix), Joseph Pecoraro LGTM-ed.', None)

    def _entry_with_author(self, author_text):
        return ChangeLogEntry('''2009-08-19  AUTHOR_TEXT

            Reviewed by Ryosuke Niwa

            * Scripts/bugzilla-tool:
'''.replace("AUTHOR_TEXT", author_text))

    def _entry_with_reviewer(self, reviewer_line):
        return ChangeLogEntry('''2009-08-19  Eric Seidel  <eric@webkit.org>

            REVIEW_LINE

            * Scripts/bugzilla-tool:
'''.replace("REVIEW_LINE", reviewer_line))

    def _contributors(self, names):
        return [CommitterList().contributor_by_name(name) for name in names]

    def _assert_fuzzy_reviewer_match(self, reviewer_text, expected_text_list, expected_contributors):
        unused, reviewer_text_list = ChangeLogEntry._parse_reviewer_text(reviewer_text)
        self.assertEquals(reviewer_text_list, expected_text_list)
        self.assertEquals(self._entry_with_reviewer(reviewer_text).reviewers(), self._contributors(expected_contributors))

    def test_fuzzy_reviewer_match(self):
        self._assert_fuzzy_reviewer_match('Reviewed by Dimitri Glazkov, build fix', ['Dimitri Glazkov', 'build fix'], ['Dimitri Glazkov'])
        self._assert_fuzzy_reviewer_match('Reviewed by BUILD FIX', ['BUILD FIX'], [])
        self._assert_fuzzy_reviewer_match('Reviewed by Mac build fix', ['Mac build fix'], [])
        self._assert_fuzzy_reviewer_match('Reviewed by Darin Adler, Dan Bernstein, Adele Peterson, and others.',
            ['Darin Adler', 'Dan Bernstein', 'Adele Peterson', 'others'], ['Darin Adler', 'Dan Bernstein', 'Adele Peterson'])
        self._assert_fuzzy_reviewer_match('Reviewed by George Staikos (and others)', ['George Staikos', 'others'], ['George Staikos'])
        self._assert_fuzzy_reviewer_match('Reviewed by Mark Rowe, but Dan Bernstein also reviewed and asked thoughtful questions.',
            ['Mark Rowe', 'but Dan Bernstein also reviewed', 'asked thoughtful questions'], ['Mark Rowe'])
        self._assert_fuzzy_reviewer_match('Reviewed by Darin Adler in <https://bugs.webkit.org/show_bug.cgi?id=47736>.', ['Darin Adler in'], ['Darin Adler'])
        self._assert_fuzzy_reviewer_match('Reviewed by Adam Barth.:w', ['Adam Barth.:w'], ['Adam Barth'])

    def _assert_parse_authors(self, author_text, expected_contributors):
        parsed_authors = [(author['name'], author['email']) for author in self._entry_with_author(author_text).authors()]
        self.assertEquals(parsed_authors, expected_contributors)

    def test_parse_authors(self):
        self._assert_parse_authors(u'Aaron Colwell  <acolwell@chromium.org>', [(u'Aaron Colwell', u'acolwell@chromium.org')])
        self._assert_parse_authors('Eric Seidel  <eric@webkit.org>, Ryosuke Niwa  <rniwa@webkit.org>',
            [('Eric Seidel', 'eric@webkit.org'), ('Ryosuke Niwa', 'rniwa@webkit.org')])
        self._assert_parse_authors('Zan Dobersek  <zandobersek@gmail.com> and Philippe Normand  <pnormand@igalia.com>',
            [('Zan Dobersek', 'zandobersek@gmail.com'), ('Philippe Normand', 'pnormand@igalia.com')])
        self._assert_parse_authors('New Contributor  <new@webkit.org> and Noob  <noob@webkit.org>',
            [('New Contributor', 'new@webkit.org'), ('Noob', 'noob@webkit.org')])

    def _assert_has_valid_reviewer(self, reviewer_line, expected):
        self.assertEqual(self._entry_with_reviewer(reviewer_line).has_valid_reviewer(), expected)

    def test_has_valid_reviewer(self):
        self._assert_has_valid_reviewer("Reviewed by Eric Seidel.", True)
        self._assert_has_valid_reviewer("Reviewed by Eric Seidel", True)  # Not picky about the '.'
        self._assert_has_valid_reviewer("Reviewed by Eric.", False)
        self._assert_has_valid_reviewer("Reviewed by Eric C Seidel.", False)
        self._assert_has_valid_reviewer("Rubber-stamped by Eric.", False)
        self._assert_has_valid_reviewer("Rubber-stamped by Eric Seidel.", True)
        self._assert_has_valid_reviewer("Rubber stamped by Eric.", False)
        self._assert_has_valid_reviewer("Rubber stamped by Eric Seidel.", True)
        self._assert_has_valid_reviewer("Unreviewed build fix.", True)

    def test_latest_entry_parse(self):
        changelog_contents = u"%s\n%s" % (self._example_entry, self._example_changelog)
        changelog_file = StringIO(changelog_contents)
        latest_entry = ChangeLog.parse_latest_entry_from_file(changelog_file)
        self.assertEquals(latest_entry.contents(), self._example_entry)
        self.assertEquals(latest_entry.author_name(), "Peter Kasting")
        self.assertEquals(latest_entry.author_email(), "pkasting@google.com")
        self.assertEquals(latest_entry.reviewer_text(), u"Tor Arne Vestb\xf8")
        self.assertEquals(latest_entry.touched_files(), ["DumpRenderTree/win/DumpRenderTree.vcproj", "DumpRenderTree/win/ImageDiff.vcproj", "DumpRenderTree/win/TestNetscapePlugin/TestNetscapePlugin.vcproj"])

        self.assertTrue(latest_entry.reviewer())  # Make sure that our UTF8-based lookup of Tor works.

    def test_latest_entry_parse_single_entry(self):
        changelog_contents = u"%s\n%s" % (self._example_entry, self._rolled_over_footer)
        changelog_file = StringIO(changelog_contents)
        latest_entry = ChangeLog.parse_latest_entry_from_file(changelog_file)
        self.assertEquals(latest_entry.contents(), self._example_entry)
        self.assertEquals(latest_entry.author_name(), "Peter Kasting")

    @staticmethod
    def _write_tmp_file_with_contents(byte_array):
        assert(isinstance(byte_array, str))
        (file_descriptor, file_path) = tempfile.mkstemp() # NamedTemporaryFile always deletes the file on close in python < 2.6
        with os.fdopen(file_descriptor, "w") as file:
            file.write(byte_array)
        return file_path

    @staticmethod
    def _read_file_contents(file_path, encoding):
        with codecs.open(file_path, "r", encoding) as file:
            return file.read()

    # FIXME: We really should be getting this from prepare-ChangeLog itself.
    _new_entry_boilerplate = '''2009-08-19  Eric Seidel  <eric@webkit.org>

        Need a short description and bug URL (OOPS!)

        Reviewed by NOBODY (OOPS!).

        * Scripts/bugzilla-tool:
'''

    def test_set_reviewer(self):
        changelog_contents = u"%s\n%s" % (self._new_entry_boilerplate, self._example_changelog)
        changelog_path = self._write_tmp_file_with_contents(changelog_contents.encode("utf-8"))
        reviewer_name = 'Test Reviewer'
        ChangeLog(changelog_path).set_reviewer(reviewer_name)
        actual_contents = self._read_file_contents(changelog_path, "utf-8")
        expected_contents = changelog_contents.replace('NOBODY (OOPS!)', reviewer_name)
        os.remove(changelog_path)
        self.assertEquals(actual_contents.splitlines(), expected_contents.splitlines())

    def test_set_short_description_and_bug_url(self):
        changelog_contents = u"%s\n%s" % (self._new_entry_boilerplate, self._example_changelog)
        changelog_path = self._write_tmp_file_with_contents(changelog_contents.encode("utf-8"))
        short_description = "A short description"
        bug_url = "http://example.com/b/2344"
        ChangeLog(changelog_path).set_short_description_and_bug_url(short_description, bug_url)
        actual_contents = self._read_file_contents(changelog_path, "utf-8")
        expected_message = "%s\n        %s" % (short_description, bug_url)
        expected_contents = changelog_contents.replace("Need a short description and bug URL (OOPS!)", expected_message)
        os.remove(changelog_path)
        self.assertEquals(actual_contents.splitlines(), expected_contents.splitlines())

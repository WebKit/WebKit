# Copyright (C) 2026 Apple Inc. All rights reserved.
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

import os
import re
import shutil
import tempfile
import unittest
from unittest.mock import MagicMock, patch

from webkitcorepy import OutputCapture, Terminal
from webkitscmpy import Commit, local
from webkitscmpy.program.diff.diff import DiffBase
from webkitscmpy.program.diff.html_diff import HTMLDiff


class _Commit(object):
    def __init__(self, identifier):
        self.identifier = identifier

    def __str__(self):
        return self.identifier


class TestDiffBase(unittest.TestCase):
    NULL_SHA = '0' * 40
    RESOLVED_SHA = 'a' * 40

    # add_lines() feeds add_line() the output of str.splitlines(), which strips the trailing
    # newline; add_line() adds one back. Inputs here omit it to match that real call path.
    @staticmethod
    def _from_line(sha):
        return 'From {} Mon Sep 17 00:00:00 2001'.format(sha)

    def _repo(self, commit=None, unresolvable=False):
        repository = MagicMock(spec=local.Git)
        repository.Exception = local.Scm.Exception
        if unresolvable:
            repository.commit.side_effect = local.Scm.Exception('no such commit')
        else:
            repository.commit.return_value = commit
        return repository

    def test_from_line_rewritten_when_hash_resolves(self):
        diff = DiffBase(repository=self._repo(commit=_Commit('271828@main')))
        result = diff.add_line(self._from_line(self.RESOLVED_SHA))
        self.assertEqual(result, 'From 271828@main ({})\n'.format(self.RESOLVED_SHA[:Commit.HASH_LABEL_SIZE]))

    def test_from_line_falls_back_when_hash_unresolvable(self):
        diff = DiffBase(repository=self._repo(unresolvable=True))
        line = self._from_line(self.NULL_SHA)
        self.assertEqual(diff.add_line(line), line + '\n')

    def test_from_line_ignored_without_repository(self):
        diff = DiffBase(repository=None)
        line = self._from_line(self.NULL_SHA)
        self.assertEqual(diff.add_line(line), line + '\n')


class TestHTMLDiff(unittest.TestCase):
    DIFF = [
        'From 8b3ae7d0f9b5c9b3e1fb9b3fd3c3f4b5a6d7e8f9 Mon Sep 17 00:00:00 2001',
        'From: Jonathan Bedard <jbedard@apple.com>',
        'Subject: [PATCH] Example change',
        '',
        '---',
        'diff --git a/Source/A.cpp b/Source/A.cpp',
        'index 1111111..2222222 100644',
        '--- a/Source/A.cpp',
        '+++ b/Source/A.cpp',
        '@@ -1,2 +1,2 @@ void a()',
        ' context',
        '-if (a < b)',
        '+if (a <= b) // </script>',
        'diff --git a/Source/B.cpp b/Source/B.cpp',
        'new file mode 100644',
        'index 0000000..3333333',
        '--- /dev/null',
        '+++ b/Source/B.cpp',
        '@@ -0,0 +1,1 @@',
        '+int b;',
    ]

    def setUp(self):
        self.directory = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.directory, ignore_errors=True)

    def render(self):
        with patch.object(Terminal, 'open_url'):
            diff = HTMLDiff(block=False)
            diff.file = os.path.join(self.directory, 'diff.html')
            with diff:
                diff.add_lines(self.DIFF)
        with open(diff.file) as file:
            return file.read()

    def test_files_collapsed(self):
        html = self.render()
        self.assertEqual(html.count("<details class='file'>"), 2)
        self.assertNotIn('<details open', html)
        self.assertIn('<summary><h1>Source/A.cpp</h1></summary>', html)
        self.assertIn('<summary><h1>Source/B.cpp</h1></summary>', html)

    def test_file_contents_deferred(self):
        html = self.render()
        deferred = re.findall(r"<script type='text/html'>(.*?)</script>", html, re.DOTALL)
        self.assertEqual(len(deferred), 2)
        self.assertIn('if (a &lt;= b) // &lt;/script&gt;', deferred[0])
        self.assertIn('int b;', deferred[1])
        self.assertEqual(html.count('</script>'), len(deferred) + 1)

    def test_moves_and_copies_not_collapsible(self):
        self.DIFF = [
            'diff --git a/Source/Old.cpp b/Source/New.cpp',
            'similarity index 100%',
            'rename from Source/Old.cpp',
            'rename to Source/New.cpp',
            'diff --git a/Source/A.cpp b/Source/C.cpp',
            'similarity index 100%',
            'copy from Source/A.cpp',
            'copy to Source/C.cpp',
            'diff --git a/Source/B.cpp b/Source/B.cpp',
            'index 1111111..2222222 100644',
            '--- a/Source/B.cpp',
            '+++ b/Source/B.cpp',
            '@@ -1,1 +1,1 @@',
            '-int b;',
            '+int c;',
        ]
        html = self.render()
        self.assertIn("<div class='file empty'>\n    <h1>Source/Old.cpp -&gt; Source/New.cpp</h1>", html)
        self.assertIn("<div class='file empty'>\n    <h1>Source/A.cpp -&gt; Source/C.cpp</h1>", html)
        self.assertEqual(html.count("<details class='file'>"), 1)
        self.assertIn('<summary><h1>Source/B.cpp</h1></summary>', html)

    def test_single_line_section(self):
        self.DIFF = [
            'diff --git a/Source/A.html b/Source/A.html',
            'new file mode 100644',
            'index 0000000..1111111',
            '--- /dev/null',
            '+++ b/Source/A.html',
            '@@ -0,0 +1 @@',
            '+<!-- comment -->',
        ]
        html = self.render()
        self.assertNotIn('file empty', html)
        self.assertIn('<summary><h1>Source/A.html</h1></summary>', html)
        self.assertIn('&lt;!-- comment --&gt;', html)

    def test_context_lines_resembling_headers(self):
        self.DIFF = [
            'diff --git a/Source/A.cpp b/Source/A.cpp',
            'index 1111111..2222222 100644',
            '--- a/Source/A.cpp',
            '+++ b/Source/A.cpp',
            '@@ -1,4 +1,4 @@',
            ' index = 0;',
            ' diff = a - b;',
            ' From here',
            '-int a;',
            '+int b;',
        ]
        with OutputCapture() as captured:
            html = self.render()
        self.assertEqual(captured.stderr.getvalue(), '')
        deferred = re.findall(r"<script type='text/html'>(.*?)</script>", html, re.DOTALL)
        self.assertEqual(len(deferred), 1)
        for text in ('index = 0;', 'diff = a - b;', 'From here', 'int a;', 'int b;'):
            self.assertIn(text, deferred[0])

    def test_same_file_in_consecutive_commits(self):
        self.DIFF = [
            'From 1111111111111111111111111111111111111111 Mon Sep 17 00:00:00 2001',
            'Subject: [PATCH 1/2] First',
            '',
            '---',
            'diff --git a/Source/A.cpp b/Source/A.cpp',
            '--- a/Source/A.cpp',
            '+++ b/Source/A.cpp',
            '@@ -1,1 +1,1 @@',
            '-int a;',
            '+int b;',
            'From 2222222222222222222222222222222222222222 Mon Sep 17 00:00:00 2001',
            'Subject: [PATCH 2/2] Second',
            '',
            '---',
            'diff --git a/Source/A.cpp b/Source/A.cpp',
            '--- a/Source/A.cpp',
            '+++ b/Source/A.cpp',
            '@@ -1,1 +1,1 @@',
            '-int b;',
            '+int c;',
        ]
        html = self.render()
        self.assertEqual(html.count('<summary><h1>Source/A.cpp</h1></summary>'), 2)
        deferred = re.findall(r"<script type='text/html'>(.*?)</script>", html, re.DOTALL)
        self.assertEqual(len(deferred), 2)
        self.assertIn('int c;', deferred[1])

    def test_binary_and_mode_changes(self):
        self.DIFF = [
            'diff --git a/Source/image.png b/Source/image.png',
            'index 1111111..2222222 100644',
            'Binary files a/Source/image.png and b/Source/image.png differ',
            'diff --git a/Tools/script with space b/Tools/script with space',
            'old mode 100644',
            'new mode 100755',
            'diff --git a/Source/B.cpp b/Source/B.cpp',
            'index 1111111..2222222 100644',
            '--- a/Source/B.cpp',
            '+++ b/Source/B.cpp',
            '@@ -1,1 +1,1 @@',
            '-int b;',
            '+int c;',
        ]
        with OutputCapture() as captured:
            html = self.render()
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertIn("<div class='file empty'>\n    <h1>Source/image.png</h1>\n    <div class='file-note'>Binary file changed</div>", html)
        self.assertIn("<div class='file empty'>\n    <h1>Tools/script with space</h1>\n    <div class='file-note'>Mode changed from 100644 to 100755</div>", html)
        self.assertEqual(html.count("<details class='file'>"), 1)
        self.assertIn('<summary><h1>Source/B.cpp</h1></summary>', html)

    def test_git_binary_patch(self):
        self.DIFF = [
            'diff --git a/Source/image.png b/Source/image.png',
            'index 1111111..2222222 100644',
            'GIT binary patch',
            'delta 12',
            'zcmb7?&ubGw6vvYY4P9sxMB*&9?',
            '',
            'delta 10',
            'zj~?aS{1<u&cy7J<H+WI-;ziVhX',
            '',
            'diff --git a/Source/B.cpp b/Source/B.cpp',
            'index 1111111..2222222 100644',
            '--- a/Source/B.cpp',
            '+++ b/Source/B.cpp',
            '@@ -1,1 +1,1 @@',
            '-int b;',
            '+int c;',
        ]
        with OutputCapture() as captured:
            html = self.render()
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertIn("<div class='file empty'>\n    <h1>Source/image.png</h1>\n    <div class='file-note'>Binary file changed</div>", html)
        self.assertNotIn('zcmb7', html)
        self.assertEqual(html.count("<details class='file'>"), 1)
        self.assertIn('int c;', html)

    def test_format_patch_signature(self):
        self.DIFF = [
            'From 1111111111111111111111111111111111111111 Mon Sep 17 00:00:00 2001',
            'Subject: [PATCH 1/2] First',
            '',
            '---',
            'diff --git a/Source/A.md b/Source/A.md',
            '--- a/Source/A.md',
            '+++ b/Source/A.md',
            '@@ -1,2 +1,1 @@',
            '-- ',
            '-item',
            '+entry',
            '\\ No newline at end of file',
            '-- ',
            '2.50.1 (Apple Git-155)',
            '',
            'From 2222222222222222222222222222222222222222 Mon Sep 17 00:00:00 2001',
            'Subject: [PATCH 2/2] Second',
            '',
            '---',
            'diff --git a/Source/B.cpp b/Source/B.cpp',
            '--- a/Source/B.cpp',
            '+++ b/Source/B.cpp',
            '@@ -1 +1 @@',
            '-int b;',
            '+int c;',
            '--',
            '2.50.1 (Apple Git-155)',
            '',
        ]
        with OutputCapture() as captured:
            html = self.render()
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertNotIn('Apple Git', html)
        self.assertEqual(html.count("<div class='file commit'>"), 2)
        deferred = re.findall(r"<script type='text/html'>(.*?)</script>", html, re.DOTALL)
        self.assertEqual(len(deferred), 2)
        self.assertIn('<span class="text remove">- </span>', deferred[0])
        self.assertIn('entry', deferred[0])
        self.assertEqual(deferred[0].count('<span class="text remove">- </span>'), 1)
        self.assertIn('int c;', deferred[1])

    def test_search_box(self):
        html = self.render()
        self.assertIn("<input type='search'", html)
        self.assertLess(html.index("<input type='search'"), html.index('<details'))

    def test_commit_message_not_collapsed(self):
        html = self.render()
        self.assertIn("<div class='file commit'>", html)
        self.assertLess(html.index('Example change'), html.index('<details'))


if __name__ == '__main__':
    unittest.main()

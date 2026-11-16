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

import unittest
from unittest.mock import MagicMock

from webkitscmpy import Commit, local
from webkitscmpy.program.diff.diff import DiffBase


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


if __name__ == '__main__':
    unittest.main()

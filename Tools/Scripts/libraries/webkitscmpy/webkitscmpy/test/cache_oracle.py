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

from webkitscmpy import Commit, local


class CacheOracle(object):
    """git is the oracle for the identifier cache: a Git without a cache must give the same answer."""

    @staticmethod
    def observe(value):
        if isinstance(value, Commit):
            return (value.hash, str(value), value.branch, value.branch_point)
        if hasattr(value, '__next__'):
            return [CacheOracle.observe(element) for element in value]
        return value

    def assert_matches_uncached(self, repository, call):
        """Make `call` on `repository` and on a new uncached Git over the same path: the answers must agree.

        The uncached call is made first, so a side effect of `call` is only seen by `repository`."""
        expected = self.observe(call(local.Git(repository.path, cached=False)))
        actual = self.observe(call(repository))
        self.assertEqual(actual, expected, 'cached answer differs from uncached')
        return actual

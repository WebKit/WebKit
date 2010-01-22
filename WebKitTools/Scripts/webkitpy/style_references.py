# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""References to non-style modules used by the style package."""

# This module is a simple facade to the functionality used by the
# style package that comes from WebKit modules outside the style
# package.
#
# With this module, the only intra-package references (i.e.
# references to webkitpy modules outside the style folder) that
# the style package needs to make are relative references to
# this module. For example--
#
# > from .. style_references import parse_patch
#
# Similarly, people maintaining non-style code are not beholden
# to the contents of the style package when refactoring or
# otherwise changing non-style code. They only have to be aware
# of this module.

import os

from diff_parser import DiffParser
from scm import detect_scm_system


def parse_patch(patch_string):

    """Parse a patch string and return the affected files."""

    patch = DiffParser(patch_string.splitlines())
    return patch.files


class SimpleScm(object):

    """Simple facade to SCM for use by style package."""

    def __init__(self):
        cwd = os.path.abspath('.')
        self._scm = detect_scm_system(cwd)

    def checkout_root(self):
        """Return the source control root as an absolute path."""
        return self._scm.checkout_root

    def create_patch(self):
        return self._scm.create_patch()

    def create_patch_since_local_commit(self, commit):
        return self._scm.create_patch_since_local_commit(commit)


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

from webkitpy.common.checkout.diff_parser import DiffParser
from webkitpy.common.system.logtesting import LogTesting
from webkitpy.common.system.logtesting import TestLogStream
from webkitpy.common.system.logutils import configure_logging
from webkitpy.common.checkout.scm import detect_scm_system
from webkitpy.layout_tests import port
from webkitpy.layout_tests.layout_package import test_expectations
from webkitpy.thirdparty.autoinstalled import pep8


def detect_checkout():
    """Return a WebKitCheckout instance, or None if it cannot be found."""
    cwd = os.path.abspath(os.curdir)
    scm = detect_scm_system(cwd)

    return None if scm is None else WebKitCheckout(scm)


class WebKitCheckout(object):

    """Simple facade to the SCM class for use by style package."""

    def __init__(self, scm):
        self._scm = scm

    def root_path(self):
        """Return the checkout root as an absolute path."""
        return self._scm.checkout_root

    def create_patch(self, git_commit, changed_files=None):
        # FIXME: SCM.create_patch should understand how to handle None.
        return self._scm.create_patch(git_commit, changed_files=changed_files or [])

# Copyright (c) 2009, 2010, 2011 Google Inc. All rights reserved.
# Copyright (c) 2009, 2016 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
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

import logging
import os
import random
import re
import shutil
import string
import sys
import tempfile

from webkitcorepy import Version, string_utils

from webkitpy.common.checkout.scm.scm import AuthenticationError, SCM, commit_error_handler
from webkitpy.common.config.urls import svn_server_host, svn_server_realm
from webkitpy.common.memoized import memoized
from webkitpy.common.system.executive import Executive, ScriptError
from webkitpy.common.webkit_finder import WebKitFinder


_log = logging.getLogger(__name__)


# A mixin class that represents common functionality for SVN and Git-SVN.
class SVNRepository(object):
    svn_server_host = svn_server_host
    svn_server_realm = svn_server_realm

    def has_authorization_for_realm(self, realm, home_directory=None):
        # If we are working on a file:// repository realm will be None
        if realm is None:
            return True

        # Find the user's home directory
        if home_directory is None:
            home_directory = os.path.expanduser("~")
            if home_directory == "~":
                return False

        # ignore false positives for methods implemented in the mixee class. pylint: disable=E1101
        # Assumes find and grep are installed.
        if not os.path.isdir(os.path.join(home_directory, ".subversion")):
            return False
        find_args = ["find", ".subversion", "-type", "f", "-exec", "grep", "-q", realm, "{}", ";", "-print"]
        find_output = self.run(find_args, cwd=home_directory, ignore_errors=True).rstrip()
        if not find_output or not os.path.isfile(os.path.join(home_directory, find_output)):
            return False
        # Subversion either stores the password in the credential file, indicated by the presence of the key "password",
        # or uses the system password store (e.g. Keychain on Mac OS X) as indicated by the presence of the key "passtype".
        # We assume that these keys will not coincide with the actual credential data (e.g. that a person's username
        # isn't "password") so that we can use grep.
        if self.run(["grep", "password", find_output], cwd=home_directory, return_exit_code=True) == 0:
            return True
        return self.run(["grep", "passtype", find_output], cwd=home_directory, return_exit_code=True) == 0

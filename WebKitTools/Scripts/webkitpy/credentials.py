# Copyright (c) 2009 Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
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
#
# Python module for reading stored web credentials from the OS.

import getpass
import os
import platform
import re

from webkitpy.executive import Executive, ScriptError
from webkitpy.webkit_logging import log
from webkitpy.scm import Git
from webkitpy.user import User


class Credentials(object):

    def __init__(self, host, git_prefix=None, executive=None, cwd=os.getcwd()):
        self.host = host
        self.git_prefix = git_prefix
        self.executive = executive or Executive()
        self.cwd = cwd

    def _credentials_from_git(self):
        return [self._read_git_config("username"),
                self._read_git_config("password")]

    def _read_git_config(self, key):
        config_key = "%s.%s" % (self.git_prefix, key) if self.git_prefix \
                                                      else key
        return self.executive.run_command(
                ["git", "config", "--get", config_key],
                error_handler=Executive.ignore_error).rstrip('\n')

    def _keychain_value_with_label(self, label, source_text):
        match = re.search("%s\"(?P<value>.+)\"" % label,
                                                  source_text,
                                                  re.MULTILINE)
        if match:
            return match.group('value')

    def _is_mac_os_x(self):
        return platform.mac_ver()[0]

    def _parse_security_tool_output(self, security_output):
        username = self._keychain_value_with_label("^\s*\"acct\"<blob>=",
                                                   security_output)
        password = self._keychain_value_with_label("^password: ",
                                                   security_output)
        return [username, password]

    def _run_security_tool(self, username=None):
        security_command = [
            "/usr/bin/security",
            "find-internet-password",
            "-g",
            "-s",
            self.host,
        ]
        if username:
            security_command += ["-a", username]

        log("Reading Keychain for %s account and password.  "
            "Click \"Allow\" to continue..." % self.host)
        try:
            return self.executive.run_command(security_command)
        except ScriptError:
            # Failed to either find a keychain entry or somekind of OS-related
            # error occured (for instance, couldn't find the /usr/sbin/security
            # command).
            log("Could not find a keychain entry for %s." % self.host)
            return None

    def _credentials_from_keychain(self, username=None):
        if not self._is_mac_os_x():
            return [username, None]

        security_output = self._run_security_tool(username)
        if security_output:
            return self._parse_security_tool_output(security_output)
        else:
            return [None, None]

    def read_credentials(self):
        username = None
        password = None

        try:
            if Git.in_working_directory(self.cwd):
                (username, password) = self._credentials_from_git()
        except OSError, e:
            # Catch and ignore OSError exceptions such as "no such file 
            # or directory" (OSError errno 2), which imply that the Git
            # command cannot be found/is not installed.
            pass

        if not username or not password:
            (username, password) = self._credentials_from_keychain(username)

        if not username:
            username = User.prompt("%s login: " % self.host)
        if not password:
            password = getpass.getpass("%s password for %s: " % (self.host,
                                                                 username))

        return [username, password]

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

import os
import tempfile
import unittest
from webkitpy.common.net.credentials import Credentials
from webkitpy.common.system.executive import Executive
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.thirdparty.mock import Mock

class CredentialsTest(unittest.TestCase):
    example_security_output = """keychain: "/Users/test/Library/Keychains/login.keychain"
class: "inet"
attributes:
    0x00000007 <blob>="bugs.webkit.org (test@webkit.org)"
    0x00000008 <blob>=<NULL>
    "acct"<blob>="test@webkit.org"
    "atyp"<blob>="form"
    "cdat"<timedate>=0x32303039303832353233353231365A00  "20090825235216Z\000"
    "crtr"<uint32>=<NULL>
    "cusi"<sint32>=<NULL>
    "desc"<blob>="Web form password"
    "icmt"<blob>="default"
    "invi"<sint32>=<NULL>
    "mdat"<timedate>=0x32303039303930393137323635315A00  "20090909172651Z\000"
    "nega"<sint32>=<NULL>
    "path"<blob>=<NULL>
    "port"<uint32>=0x00000000 
    "prot"<blob>=<NULL>
    "ptcl"<uint32>="htps"
    "scrp"<sint32>=<NULL>
    "sdmn"<blob>=<NULL>
    "srvr"<blob>="bugs.webkit.org"
    "type"<uint32>=<NULL>
password: "SECRETSAUCE"
"""

    def test_keychain_lookup_on_non_mac(self):
        class FakeCredentials(Credentials):
            def _is_mac_os_x(self):
                return False
        credentials = FakeCredentials("bugs.webkit.org")
        self.assertEqual(credentials._is_mac_os_x(), False)
        self.assertEqual(credentials._credentials_from_keychain("foo"), ["foo", None])

    def test_security_output_parse(self):
        credentials = Credentials("bugs.webkit.org")
        self.assertEqual(credentials._parse_security_tool_output(self.example_security_output), ["test@webkit.org", "SECRETSAUCE"])

    def test_security_output_parse_entry_not_found(self):
        credentials = Credentials("foo.example.com")
        if not credentials._is_mac_os_x():
            return # This test does not run on a non-Mac.

        # Note, we ignore the captured output because it is already covered
        # by the test case CredentialsTest._assert_security_call (below).
        outputCapture = OutputCapture()
        outputCapture.capture_output()
        self.assertEqual(credentials._run_security_tool(), None)
        outputCapture.restore_output()

    def _assert_security_call(self, username=None):
        executive_mock = Mock()
        credentials = Credentials("example.com", executive=executive_mock)

        expected_stderr = "Reading Keychain for example.com account and password.  Click \"Allow\" to continue...\n"
        OutputCapture().assert_outputs(self, credentials._run_security_tool, [username], expected_stderr=expected_stderr)

        security_args = ["/usr/bin/security", "find-internet-password", "-g", "-s", "example.com"]
        if username:
            security_args += ["-a", username]
        executive_mock.run_command.assert_called_with(security_args)

    def test_security_calls(self):
        self._assert_security_call()
        self._assert_security_call(username="foo")

    def test_read_credentials_without_git_repo(self):
        class FakeCredentials(Credentials):
            def _is_mac_os_x(self):
                return True
            def _credentials_from_keychain(self, username):
                return ["test@webkit.org", "SECRETSAUCE"]

        temp_dir_path = tempfile.mkdtemp(suffix="not_a_git_repo")
        credentials = FakeCredentials("bugs.webkit.org", cwd=temp_dir_path)
        self.assertEqual(credentials.read_credentials(), ["test@webkit.org", "SECRETSAUCE"])
        os.rmdir(temp_dir_path)

    def test_keyring_without_git_repo(self):
        class MockKeyring(object):
            def get_password(self, host, username):
                return "NOMNOMNOM"

        class FakeCredentials(Credentials):
            def __init__(self, cwd):
                Credentials.__init__(self, "fake.hostname", cwd=cwd,
                                     keyring=MockKeyring())

            def _is_mac_os_x(self):
                return True

            def _credentials_from_keychain(self, username):
                return ("test@webkit.org", None)

        temp_dir_path = tempfile.mkdtemp(suffix="not_a_git_repo")
        credentials = FakeCredentials(temp_dir_path)
        try:
            self.assertEqual(credentials.read_credentials(), ["test@webkit.org", "NOMNOMNOM"])
        finally:
            os.rmdir(temp_dir_path)

if __name__ == '__main__':
    unittest.main()

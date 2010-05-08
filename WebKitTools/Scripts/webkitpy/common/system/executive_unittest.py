# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2009 Daniel Bates (dbates@intudata.com). All rights reserved.
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

import signal
import subprocess
import sys
import unittest

from webkitpy.common.system.executive import Executive, run_command


class ExecutiveTest(unittest.TestCase):

    def test_run_command_with_bad_command(self):
        def run_bad_command():
            run_command(["foo_bar_command_blah"], error_handler=Executive.ignore_error, return_exit_code=True)
        self.failUnlessRaises(OSError, run_bad_command)

    def test_run_command_with_unicode(self):
        """Validate that it is safe to pass unicode() objects
        to Executive.run* methods, and they will return unicode()
        objects by default unless decode_output=False"""
        executive = Executive()
        unicode_tor = u"WebKit \u2661 Tor Arne Vestb\u00F8!"
        utf8_tor = unicode_tor.encode("utf-8")

        output = executive.run_command(["cat"], input=unicode_tor)
        self.assertEquals(output, unicode_tor)

        output = executive.run_command(["echo", "-n", unicode_tor])
        self.assertEquals(output, unicode_tor)

        output = executive.run_command(["echo", "-n", unicode_tor], decode_output=False)
        self.assertEquals(output, utf8_tor)

        # Make sure that str() input also works.
        output = executive.run_command(["cat"], input=utf8_tor, decode_output=False)
        self.assertEquals(output, utf8_tor)

        # FIXME: We should only have one run* method to test
        output = executive.run_and_throw_if_fail(["echo", "-n", unicode_tor], quiet=True)
        self.assertEquals(output, unicode_tor)

        output = executive.run_and_throw_if_fail(["echo", "-n", unicode_tor], quiet=True, decode_output=False)
        self.assertEquals(output, utf8_tor)

    def test_kill_process(self):
        executive = Executive()
        # FIXME: This may need edits to work right on windows.
        # We use "yes" because it loops forever.
        process = subprocess.Popen(["yes"], stdout=subprocess.PIPE)
        self.assertEqual(process.poll(), None)  # Process is running
        executive.kill_process(process.pid)
        self.assertEqual(process.wait(), -signal.SIGKILL)
        # Killing again should fail silently.
        executive.kill_process(process.pid)

    def test_kill_all(self):
        executive = Executive()
        # FIXME: This may need edits to work right on windows.
        # We use "yes" because it loops forever.
        process = subprocess.Popen(["yes"], stdout=subprocess.PIPE)
        self.assertEqual(process.poll(), None)  # Process is running
        executive.kill_all("yes")
        self.assertEqual(process.wait(), -signal.SIGTERM)
        # Killing again should fail silently.
        executive.kill_all("yes")

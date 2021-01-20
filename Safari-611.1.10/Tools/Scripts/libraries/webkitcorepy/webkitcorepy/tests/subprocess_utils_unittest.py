# Copyright (C) 2020 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
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

import sys
import unittest

from webkitcorepy import OutputCapture, run, TimeoutExpired, Timeout


class SubprocessUtils(unittest.TestCase):

    def test_run(self):
        result = run([sys.executable, '-c', 'print("message")'], capture_output=True, encoding='utf-8')
        self.assertEqual(0, result.returncode)
        self.assertEqual(result.stdout, 'message\n')
        self.assertEqual(result.stderr, '')

    def test_run_exit(self):
        result = run([sys.executable, '-c', 'import sys;sys.exit(1)'])
        self.assertEqual(1, result.returncode)
        self.assertEqual(result.stdout, None)
        self.assertEqual(result.stderr, None)

    def test_run_timeout(self):
        with OutputCapture(), self.assertRaises(TimeoutExpired):
            run([sys.executable, '-c', 'import time;time.sleep(2)'], timeout=1)

    def test_run_timeout_context(self):
        with OutputCapture(), self.assertRaises(TimeoutExpired):
            with Timeout(1):
                run([sys.executable, '-c', 'import time;time.sleep(2)'])

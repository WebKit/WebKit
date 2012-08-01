# Copyright (C) 2012 Google, Inc.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging
import unittest
import StringIO

from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.test.main import Tester


class TesterTest(unittest.TestCase):

    def test_no_tests_found(self):
        tester = Tester()
        errors = StringIO.StringIO()

        # Here we need to remove any existing log handlers so that they
        # don't log the messages webkitpy.test while we're testing it.
        root_logger = logging.getLogger()
        root_handlers = root_logger.handlers
        root_logger.handlers = []

        tester.printer.stream = errors
        tester.finder.find_names = lambda args, run_all: []
        oc = OutputCapture()
        try:
            oc.capture_output()
            self.assertFalse(tester.run())
        finally:
            _, _, logs = oc.restore_output()
            root_logger.handlers = root_handlers

        self.assertTrue('No tests to run' in errors.getvalue())
        self.assertTrue('No tests to run' in logs)

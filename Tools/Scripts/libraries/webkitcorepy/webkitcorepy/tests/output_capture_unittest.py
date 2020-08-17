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

import logging
import sys
import unittest

from webkitcorepy import log, LoggerCapture, OutputCapture, OutputDuplicate


class LoggerCaptureTest(unittest.TestCase):
    def test_basic(self):
        with LoggerCapture(log) as capturer, LoggerCapture():
            log.info('Hidden')
            log.warn('Printed')

        self.assertEqual(capturer.log.getvalue(), 'Printed\n')

    def test_level(self):
        with LoggerCapture(log, level=logging.INFO) as capturer, LoggerCapture():
            log.debug('Hidden')
            log.info('Printed 1')
            log.warn('Printed 2')

        self.assertEqual(capturer.log.getvalue(), 'Printed 1\nPrinted 2\n')

    def test_multiple_entry(self):
        with LoggerCapture(log, level=logging.INFO) as capturer, LoggerCapture():
            log.info('Level 1')
            with capturer:
                log.info('Level 2')

        self.assertEqual(capturer.log.getvalue(), 'Level 1\nLevel 2\n')


class OutputCaptureTest(unittest.TestCase):
    def test_basic(self):
        with OutputCapture() as capturer:
            log.info('Hidden')
            log.warn('Printed')
            sys.stdout.write('stdout\n')
            sys.stderr.write('stderr\n')

        self.assertEqual(capturer.webkitcorepy.log.getvalue(), 'Printed\n')
        self.assertEqual(capturer.stdout.getvalue(), 'stdout\n')
        self.assertEqual(capturer.stderr.getvalue(), 'stderr\n')

    def test_multiple_entry(self):
        with OutputCapture() as captured:
            sys.stdout.write('Line 1\n')
            log.warn('Log 1')
            with captured:
                sys.stdout.write('Line 2\n')
                log.warn('Log 2')

        self.assertEqual(captured.webkitcorepy.log.getvalue(), 'Log 1\nLog 2\n')
        self.assertEqual(captured.stdout.getvalue(), 'Line 1\nLine 2\n')


class OutputDuplicateTest(unittest.TestCase):
    def test_basic(self):
        with OutputCapture() as capturer:
            with OutputDuplicate() as duplicator:
                log.info('Hidden')
                log.warn('Printed')
                sys.stdout.write('stdout\n')
                sys.stderr.write('stderr\n')

            self.assertEqual(duplicator.output.getvalue(), 'Printed\nstdout\nstderr\n')

        self.assertEqual(capturer.webkitcorepy.log.getvalue(), 'Printed\n')
        self.assertEqual(capturer.stdout.getvalue(), 'stdout\n')
        self.assertEqual(capturer.stderr.getvalue(), 'stderr\n')

    def test_multiple_entry(self):
        with OutputCapture(level=logging.INFO) as captuered:
            with OutputDuplicate() as duplicator:
                log.info('Log 1')
                sys.stdout.write('Level 1\n')
                with duplicator:
                    log.info('Log 2')
                    sys.stdout.write('Level 2\n')

            self.assertEqual(duplicator.output.getvalue(), 'Log 1\nLevel 1\nLog 2\nLog 2\nLevel 2\nLevel 2\n')

        self.assertEqual(captuered.webkitcorepy.log.getvalue(), 'Log 1\nLog 2\n')
        self.assertEqual(captuered.stdout.getvalue(), 'Level 1\nLevel 2\n')

# Copyright (C) 2026 Apple Inc. All rights reserved.
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

import unittest

from webkitcorepy import StringIO

from webkitpy.common.program_status import ProgramStatus
from webkitpy.common.program_status_mock import TTYStringIO, encoded


class ProgramStatusTest(unittest.TestCase):
    def test_report_writes_osc_7501_sequence(self):
        stream = TTYStringIO()
        ProgramStatus(stream, 'my-tool').report('working', progress=40, msg='Almost there')
        self.assertEqual(stream.getvalue(), '\x1b]7501;state=working:progress=40:app=my-tool:msg={}\x1b\\'.format(encoded('Almost there')))

    def test_report_omits_unset_fields(self):
        stream = TTYStringIO()
        ProgramStatus(stream, 'my-tool').report('idle')
        self.assertEqual(stream.program_status_reports(), ['state=idle:app=my-tool'])

    def test_report_encodes_non_ascii_messages(self):
        stream = TTYStringIO()
        ProgramStatus(stream, 'my-tool').report('error', msg='Échec')
        self.assertEqual(stream.program_status_reports(), ['state=error:app=my-tool:msg={}'.format(encoded('Échec'))])

    def test_report_skips_repeated_reports(self):
        stream = TTYStringIO()
        status = ProgramStatus(stream, 'my-tool')
        status.report('working', progress=10)
        status.report('working', progress=10)
        status.report('working', progress=20)
        status.report('working', progress=10)
        self.assertEqual(stream.program_status_reports(), [
            'state=working:progress=10:app=my-tool',
            'state=working:progress=20:app=my-tool',
            'state=working:progress=10:app=my-tool',
        ])

    def test_report_is_silent_without_a_tty(self):
        stream = StringIO()
        ProgramStatus(stream, 'my-tool').report('working', progress=10)
        self.assertEqual(stream.getvalue(), '')

    def test_report_is_silent_without_a_stream(self):
        ProgramStatus(None, 'my-tool').report('working', progress=10)

    def test_disabled_is_silent(self):
        ProgramStatus.disabled().report('working', progress=10)

    def test_close_reports_idle_when_still_working(self):
        stream = TTYStringIO()
        status = ProgramStatus(stream, 'my-tool')
        status.report('working', progress=10)
        status.close()
        status.close()
        self.assertEqual(stream.program_status_reports(), [
            'state=working:progress=10:app=my-tool',
            'state=idle:app=my-tool',
        ])

    def test_close_keeps_the_final_state(self):
        for state in ('done', 'error', 'idle'):
            with self.subTest(state=state):
                stream = TTYStringIO()
                status = ProgramStatus(stream, 'my-tool')
                status.report(state)
                status.close()
                self.assertEqual(stream.program_status_reports(), ['state={}:app=my-tool'.format(state)])

    def test_close_is_silent_without_a_report(self):
        stream = TTYStringIO()
        ProgramStatus(stream, 'my-tool').close()
        self.assertEqual(stream.program_status_reports(), [])

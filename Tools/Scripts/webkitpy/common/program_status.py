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

import base64


class ProgramStatus(object):
    """Reports program status to the terminal with OSC 7501."""

    def __init__(self, stream, app):
        self._stream = stream
        self._app = app
        self._enabled = stream is not None and stream.isatty()
        self._last_state = None
        self._last_body = None

    @classmethod
    def disabled(cls):
        return cls(None, '')

    def close(self):
        if self._last_state == 'working':
            self.report('idle')

    def report(self, state, progress=None, msg=None):
        if not self._enabled:
            return

        self._last_state = state
        fields = ['state={}'.format(state)]
        if progress is not None:
            fields.append('progress={}'.format(progress))
        fields.append('app={}'.format(self._app))
        if msg:
            fields.append('msg={}'.format(base64.b64encode(msg.encode('utf-8')).decode('ascii')))

        body = ':'.join(fields)
        if body == self._last_body:
            return
        self._last_body = body

        self._stream.write('\x1b]7501;{}\x1b\\'.format(body))
        self._stream.flush()

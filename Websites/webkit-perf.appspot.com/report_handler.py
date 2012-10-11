#!/usr/bin/env python
# Copyright (C) 2012 Google Inc. All rights reserved.
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

import webapp2
from google.appengine.ext import db

import json
import re
from datetime import datetime

from models import ReportLog
from controller import schedule_report_process


class ReportHandler(webapp2.RequestHandler):
    def post(self):
        self.response.headers['Content-Type'] = 'text/plain; charset=utf-8'

        headers = "\n".join([key + ': ' + value for key, value in self.request.headers.items()])

        # Do as best as we can to remove the password
        request_body_without_password = re.sub(r',\s*"password"\s*:\s*".+?"', '', self.request.body)
        log = ReportLog(timestamp=datetime.now(), headers=headers, payload=request_body_without_password)
        log.put()

        self._encountered_error = False

        try:
            parsedPayload = json.loads(self.request.body)
            if isinstance(parsedPayload, list):
                parsedPayload = parsedPayload[0]
            password = parsedPayload.get('password', '')
        except:
            return self._output('Failed to parse the payload as a json. Report key: %d' % log.key().id())

        builder = log.builder()
        builder != None or self._output('No builder named "%s"' % log.get_value('builder-name'))
        log.branch() != None or self._output('No branch named "%s"' % log.get_value('branch'))
        log.platform() != None or self._output('No platform named "%s"' % log.get_value('platform'))
        log.build_number() != None or self._output('Invalid build number "%s"' % log.get_value('build-number'))
        log.timestamp() != None or self._output('Invalid timestamp "%s"' % log.get_value('timestamp'))
        log.webkit_revision() != None or self._output('Invalid webkit revision "%s"' % log.get_value('webkit-revision'))

        failed = False
        if builder and not (self.bypass_authentication() or builder.authenticate(password)):
            self._output('Authentication failed')

        if not log.results_are_well_formed():
            self._output("The payload doesn't contain results or results are malformed")

        if self._encountered_error:
            return

        log.commit = True
        log.put()
        schedule_report_process(log)
        self._output("OK")

    def _output(self, message):
        self._encountered_error = True
        self.response.out.write(message + '\n')

    def bypass_authentication(self):
        return False


class AdminReportHandler(ReportHandler):
    def bypass_authentication(self):
        return True

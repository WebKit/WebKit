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
from google.appengine.ext.webapp import template

import os

from controller import schedule_report_process
from models import ReportLog


class ReportLogsHandler(webapp2.RequestHandler):
    def get(self):
        self.response.out.write(template.render('report_logs.html', {'logs': ReportLog.all()}))

    def post(self):
        commit = bool(self.request.get('commit'))
        delete = bool(self.request.get('delete'))
        if commit == delete:
            return self._error('Invalid request')

        try:
            log = ReportLog.get_by_id(int(self.request.get('id', 0)))
        except:
            return self._error('Invalid log id "%s"' % self.request.get('id', ''))

        if not log:
            return self._error('No log found for "%s"' % self.request.get('id', ''))

        if commit:
            log.commit = True
            log.put()
            schedule_report_process(log)
        else:
            log.delete()

        self.response.out.write(template.render('report_logs.html', {'logs': ReportLog.all(), 'status': 'OK'}))

    def _error(self, message):
        self.response.headers['Content-Type'] = 'text/plain; charset=utf-8'
        self.response.out.write(message + '\n')

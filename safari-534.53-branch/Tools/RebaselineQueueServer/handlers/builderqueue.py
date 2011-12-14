# Copyright (C) 2011 Google Inc. All rights reserved.
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

from urllib import unquote_plus

from google.appengine.ext import webapp
from google.appengine.ext.webapp import template
from django.utils import simplejson

from model.queueentry import QueueEntry


class QueueHandler(webapp.RequestHandler):
    def get(self, builder_name):
        self._get(unquote_plus(builder_name))

    def post(self, builder_name):
        self._post(unquote_plus(builder_name))

    def _queued_test_names(self, builder_name):
        return [entry.test for entry in QueueEntry.entries_for_builder(builder_name)]
        
    def _queue_list_url(self, builder_name):
        return '/builder/%s/queue' % builder_name


class QueueEdit(QueueHandler):
    def _get(self, builder_name):
        test_names = self._queued_test_names(builder_name)
        self.response.out.write(
            template.render("templates/builder-queue-edit.html", {
                'builder_name': builder_name,
                'queued_test_names': simplejson.dumps(test_names),
            }))


class QueueAdd(QueueHandler):
    def _post(self, builder_name):
        current_tests = set(self._queued_test_names(builder_name))
        tests = set(self.request.get_all('test')).difference(current_tests)
        
        for test in tests:
            QueueEntry.add(builder_name, test)

        self.redirect(self._queue_list_url(builder_name))


class QueueRemove(QueueHandler):
    def _post(self, builder_name):
        tests = self.request.get_all('test')

        for test in tests:
            QueueEntry.remove(builder_name, test)

        self.redirect(self._queue_list_url(builder_name))


class QueueHtml(QueueHandler):
    def _get(self, builder_name):
        self.response.out.write(
            template.render("templates/builder-queue-list.html", {
                    'builder_name': builder_name,
                    'entries': QueueEntry.entries_for_builder(builder_name),
                }))


class QueueJson(QueueHandler):
    def _get(self, builder_name):
        queue_json = {'tests': self._queued_test_names(builder_name)}
        self.response.out.write(simplejson.dumps(queue_json))

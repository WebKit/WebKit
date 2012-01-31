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
from google.appengine.api import memcache
from google.appengine.ext.webapp import template

import os

from models import Test
from models import TestResult
from models import delete_model_with_numeric_id_holder


class MergeTestsHandler(webapp2.RequestHandler):
    def get(self):
        self.response.out.write(template.render('merge_tests.yaml', {'tests': Test.all()}))

    def post(self):
        self.response.headers['Content-Type'] = 'text/plain; charset=utf-8';

        merge = Test.get_by_key_name(self.request.get('merge'))
        into = Test.get_by_key_name(self.request.get('into'))
        if not merge or not into:
            self.response.out.write('Invalid test names')
            return

        merged_results = TestResult.all()
        merged_results.filter('name =', merge.name)
        for result in merged_results:
            result.name = into.name
            result.put()

        # Just flush everyting since we rarely merge tests and we need to flush
        # dashboard, manifest, and all runs for this test here.
        memcache.flush_all()

        delete_model_with_numeric_id_holder(merge)

        self.response.out.write('OK')

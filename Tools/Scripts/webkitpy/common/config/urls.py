# Copyright (c) 2010, Google Inc. All rights reserved.
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

import re


def view_source_url(local_path):
    return "http://trac.webkit.org/browser/trunk/%s" % local_path


def view_revision_url(revision_number):
    return "http://trac.webkit.org/changeset/%s" % revision_number


def chromium_results_zip_url(builder_name):
    return 'http://build.chromium.org/f/chromium/layout_test_results/%s/layout-test-results.zip' % builder_name

chromium_lkgr_url = "http://chromium-status.appspot.com/lkgr"
contribution_guidelines = "http://webkit.org/coding/contributing.html"

bug_server_host = "bugs.webkit.org"
_bug_server_regex = "https?://%s/" % re.sub('\.', '\\.', bug_server_host)
bug_server_url = "https://%s/" % bug_server_host
bug_url_long = _bug_server_regex + r"show_bug\.cgi\?id=(?P<bug_id>\d+)(&ctype=xml)?"
bug_url_short = r"https?\://webkit\.org/b/(?P<bug_id>\d+)"

buildbot_url = "http://build.webkit.org"
chromium_buildbot_url = "http://build.chromium.org/p/chromium.webkit"

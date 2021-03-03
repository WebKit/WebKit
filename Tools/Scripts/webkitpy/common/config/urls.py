# Copyright (c) 2010, Google Inc. All rights reserved.
# Copyright (c) 2019 Apple Inc. All rights reserved.
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
    return "https://trac.webkit.org/browser/trunk/%s" % local_path


def view_revision_url(revision_number):
    return 'https://commits.webkit.org/r{}'.format(revision_number)


def view_identifier_url(identifier):
    return "https://commits.webkit.org/{}".format(identifier)


contribution_guidelines = "https://webkit.org/coding/contributing.html"

bug_server_domain = "webkit.org"
bug_server_host = "bugs." + bug_server_domain
_bug_server_regex = "https?://%s/" % re.sub('\.', '\\.', bug_server_host)
bug_server_url = "https://%s/" % bug_server_host
bug_url_long = _bug_server_regex + r"show_bug\.cgi\?id=(?P<bug_id>\d+)(&ctype=xml|&excludefield=attachmentdata)*"
bug_url_short = r"https?\://%s/b/(?P<bug_id>\d+)" % bug_server_domain

attachment_url = _bug_server_regex + r"attachment\.cgi\?id=(?P<attachment_id>\d+)(&action=(?P<action>\w+))?"
direct_attachment_url = r"https?://bug-(?P<bug_id>\d+)-attachments.%s/attachment\.cgi\?id=(?P<attachment_id>\d+)" % bug_server_domain

buildbot_url = "https://build.webkit.org"

svn_server_host = "svn.webkit.org"
svn_server_realm = "<https://svn.webkit.org:443> Mac OS Forge"

ewsserver_default_host = "ews.webkit.org"


def parse_bug_id(string):
    if not string:
        return None
    match = re.search(bug_url_short, string)
    if match:
        return int(match.group('bug_id'))
    match = re.search(bug_url_long, string)
    if match:
        return int(match.group('bug_id'))
    return None


def parse_attachment_id(string):
    if not string:
        return None
    match = re.search(attachment_url, string)
    if match:
        return int(match.group('attachment_id'))
    match = re.search(direct_attachment_url, string)
    if match:
        return int(match.group('attachment_id'))
    return None

# Copyright (C) 2009 Google Inc. All rights reserved.
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
#
# WebKit's Python module for interacting with the Commit Queue status page.

# WebKit includes a built copy of BeautifulSoup in Scripts/modules
# so this import should always succeed.
from .BeautifulSoup import BeautifulSoup

try:
    from mechanize import Browser
except ImportError, e:
    print """
mechanize is required.

To install:
sudo easy_install mechanize

Or from the web:
http://wwwsearch.sourceforge.net/mechanize/
"""
    exit(1)

class StatusBot:
    default_host = "webkit-commit-queue.appspot.com"

    def __init__(self, host=default_host):
        self.statusbot_host = host
        self.statusbot_server_url = "http://%s" % self.statusbot_host
        self.update_status_url = "%s/update_status" % self.statusbot_server_url
        self.browser = Browser()

    def update_status(self, status, bug_id=None, patch_id=None):
        self.browser.open(self.update_status_url)
        self.browser.select_form(name="update_status")
        if bug_id:
            self.browser['bug_id'] = str(bug_id)
        if patch_id:
            self.browser['patch_id'] = str(patch_id)
        self.browser['status'] = status
        self.browser.submit()

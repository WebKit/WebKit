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

from webkitpy.common.net.networktransaction import NetworkTransaction
from webkitpy.common.system.deprecated_logging import log
from webkitpy.thirdparty.autoinstalled.mechanize import Browser
from webkitpy.thirdparty.BeautifulSoup import BeautifulSoup

import logging
import urllib2


_log = logging.getLogger("webkitpy.common.net.statusserver")


class StatusServer:
    default_host = "queues.webkit.org"

    def __init__(self, host=default_host, browser=None, bot_id=None):
        self.set_host(host)
        self._browser = browser or Browser()
        self.set_bot_id(bot_id)

    def set_host(self, host):
        self.host = host
        self.url = "http://%s" % self.host

    def set_bot_id(self, bot_id):
        self.bot_id = bot_id

    def results_url_for_status(self, status_id):
        return "%s/results/%s" % (self.url, status_id)

    def _add_patch(self, patch):
        if not patch:
            return
        if patch.bug_id():
            self._browser["bug_id"] = unicode(patch.bug_id())
        if patch.id():
            self._browser["patch_id"] = unicode(patch.id())

    def _add_results_file(self, results_file):
        if not results_file:
            return
        self._browser.add_file(results_file, "text/plain", "results.txt", 'results_file')

    def _post_status_to_server(self, queue_name, status, patch, results_file):
        if results_file:
            # We might need to re-wind the file if we've already tried to post it.
            results_file.seek(0)

        update_status_url = "%s/update-status" % self.url
        self._browser.open(update_status_url)
        self._browser.select_form(name="update_status")
        self._browser["queue_name"] = queue_name
        if self.bot_id:
            self._browser["bot_id"] = self.bot_id
        self._add_patch(patch)
        self._browser["status"] = status
        self._add_results_file(results_file)
        return self._browser.submit().read()  # This is the id of the newly created status object.

    def _post_svn_revision_to_server(self, svn_revision_number, broken_bot):
        update_svn_revision_url = "%s/update-svn-revision" % self.url
        self._browser.open(update_svn_revision_url)
        self._browser.select_form(name="update_svn_revision")
        self._browser["number"] = unicode(svn_revision_number)
        self._browser["broken_bot"] = broken_bot
        return self._browser.submit().read()

    def _post_work_items_to_server(self, queue_name, work_items):
        update_work_items_url = "%s/update-work-items" % self.url
        self._browser.open(update_work_items_url)
        self._browser.select_form(name="update_work_items")
        self._browser["queue_name"] = queue_name
        work_items = map(unicode, work_items)  # .join expects strings
        self._browser["work_items"] = " ".join(work_items)
        return self._browser.submit().read()

    def next_work_item(self, queue_name):
        _log.debug("Fetching next work item for %s" % queue_name)
        patch_status_url = "%s/next-patch/%s" % (self.url, queue_name)
        return self._fetch_url(patch_status_url)

    def update_work_items(self, queue_name, work_items):
        _log.debug("Recording work items: %s for %s" % (work_items, queue_name))
        return NetworkTransaction().run(lambda: self._post_work_items_to_server(queue_name, work_items))

    def update_status(self, queue_name, status, patch=None, results_file=None):
        log(status)
        return NetworkTransaction().run(lambda: self._post_status_to_server(queue_name, status, patch, results_file))

    def update_svn_revision(self, svn_revision_number, broken_bot):
        log("SVN revision: %s broke %s" % (svn_revision_number, broken_bot))
        return NetworkTransaction().run(lambda: self._post_svn_revision_to_server(svn_revision_number, broken_bot))

    def _fetch_url(self, url):
        try:
            return urllib2.urlopen(url).read()
        except urllib2.HTTPError, e:
            if e.code == 404:
                return None
            raise e

    def patch_status(self, queue_name, patch_id):
        patch_status_url = "%s/patch-status/%s/%s" % (self.url, queue_name, patch_id)
        return self._fetch_url(patch_status_url)

    def svn_revision(self, svn_revision_number):
        svn_revision_url = "%s/svn-revision/%s" % (self.url, svn_revision_number)
        return self._fetch_url(svn_revision_url)

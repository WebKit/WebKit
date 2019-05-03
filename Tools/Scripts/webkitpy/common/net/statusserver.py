# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2018, 2019 Apple Inc. All rights reserved.
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
# This the client designed to talk to Tools/QueueStatusServer.

from webkitpy.common.config.urls import statusserver_default_host
from webkitpy.common.net.bugzilla.attachment import Attachment
from webkitpy.common.net.networktransaction import NetworkTransaction
from webkitpy.thirdparty.BeautifulSoup import BeautifulSoup

import StringIO
import logging
import urllib2

_log = logging.getLogger(__name__)


class StatusServer:
    _AUTHORIZATION_HEADER_NAME = 'Authorization'

    def __init__(self, host=statusserver_default_host, use_https=True, browser=None, bot_id=None):
        self.set_host(host)
        self.set_use_https(use_https)
        self._api_key = ''
        from webkitpy.thirdparty.autoinstalled.mechanize import Browser
        self._browser = browser or Browser()
        self._browser.set_handle_robots(False)
        self.set_bot_id(bot_id)

    def set_host(self, host):
        self.host = host

    def set_use_https(self, use_https):
        self.use_https = bool(use_https)

    def set_bot_id(self, bot_id):
        self.bot_id = bot_id

    def set_api_key(self, api_key):
        self._api_key = str(api_key)
        new_headers = filter(lambda header: header[0] != self._AUTHORIZATION_HEADER_NAME, self._browser.addheaders)
        if api_key:
            new_headers.append(self._authorization_header_name_and_value_pair())
        self._browser.addheaders = new_headers

    def _server_url(self):
        return '{}://{}'.format(('https' if self.use_https else 'http'), self.host)

    def _authorization_header_name_and_value_pair(self):
        return (self._AUTHORIZATION_HEADER_NAME, 'apikey ' + self._api_key)

    def results_url_for_status(self, status_id):
        return '{}/results/{}'.format(self._server_url(), status_id)

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

    # 500 is the AppEngine limit for TEXT fields (which most of our fields are).
    # Exceeding the limit will result in a 500 error from the server.
    def _set_field(self, field_name, value, limit=500):
        if len(value) > limit:
            _log.warn("Attempted to set %s to value exceeding %s characters, truncating." % (field_name, limit))
        self._browser[field_name] = value[:limit]

    def _post_status_to_server(self, queue_name, status, patch, results_file):
        if results_file:
            # We might need to re-wind the file if we've already tried to post it.
            results_file.seek(0)

        update_status_url = '{}/update-status'.format(self._server_url())
        self._browser.open(update_status_url)
        self._browser.select_form(name="update_status")
        self._browser["queue_name"] = queue_name
        if self.bot_id:
            self._browser["bot_id"] = self.bot_id
        self._add_patch(patch)
        self._set_field("status", status, limit=500)
        self._add_results_file(results_file)
        return self._browser.submit().read()  # This is the id of the newly created status object.

    def _post_svn_revision_to_server(self, svn_revision_number, broken_bot):
        update_svn_revision_url = '{}/update-svn-revision'.format(self._server_url())
        self._browser.open(update_svn_revision_url)
        self._browser.select_form(name="update_svn_revision")
        self._browser["number"] = unicode(svn_revision_number)
        self._browser["broken_bot"] = broken_bot
        return self._browser.submit().read()

    def _post_work_items_to_server(self, queue_name, high_priority_work_items, work_items):
        update_work_items_url = '{}/update-work-items'.format(self._server_url())
        self._browser.open(update_work_items_url)
        self._browser.select_form(name="update_work_items")
        self._browser["queue_name"] = queue_name
        work_items = map(unicode, work_items)  # .join expects strings
        self._browser["work_items"] = " ".join(work_items)
        high_priority_work_items = map(unicode, high_priority_work_items)
        self._browser["high_priority_work_items"] = " ".join(high_priority_work_items)
        return self._browser.submit().read()

    def _upload_attachment_to_server(self, attachment_id, attachment_metadata, attachment_data):
        upload_attachment_url = '{}/upload-attachment'.format(self._server_url())
        self._browser.open(upload_attachment_url)
        self._browser.select_form(name='upload_attachment')
        self._browser['attachment_id'] = unicode(attachment_id)
        self._browser.add_file(StringIO.StringIO(unicode(attachment_metadata)), 'application/json', 'attachment-{}-metadata.json'.format(attachment_id), 'attachment_metadata')
        if isinstance(attachment_data, unicode):
            attachment_data = attachment_data.encode('utf-8')
        self._browser.add_file(StringIO.StringIO(attachment_data), 'text/plain', 'attachment-{}.patch'.format(attachment_id), 'attachment_data')
        self._browser.submit()

    def upload_attachment(self, attachment):
        _log.info('Uploading attachment {} to status server'.format(attachment.id()))
        # FIXME: Remove argument convert_404_to_None once we update AppEngine to support uploading attachments.
        return NetworkTransaction(convert_404_to_None=True).run(lambda: self._upload_attachment_to_server(attachment.id(), attachment.to_json(), attachment.contents()))

    def _post_work_item_to_ews(self, attachment_id):
        submit_to_ews_url = '{}/submit-to-ews'.format(self._server_url())
        self._browser.open(submit_to_ews_url)
        self._browser.select_form(name="submit_to_ews")
        self._browser["attachment_id"] = unicode(attachment_id)
        self._browser.submit()

    def submit_to_ews(self, attachment_id):
        _log.info("Submitting attachment %s to old EWS queues" % attachment_id)
        return NetworkTransaction().run(lambda: self._post_work_item_to_ews(attachment_id))

    def next_work_item(self, queue_name):
        _log.info("Fetching next work item for %s" % queue_name)
        next_patch_url = '{}/next-patch/{}'.format(self._server_url(), queue_name)
        return self._fetch_url(next_patch_url)

    def _post_release_work_item(self, queue_name, patch):
        release_patch_url = '{}/release-patch'.format(self._server_url())
        self._browser.open(release_patch_url)
        self._browser.select_form(name="release_patch")
        self._browser["queue_name"] = queue_name
        self._browser["attachment_id"] = unicode(patch.id())
        self._browser.submit()

    def release_work_item(self, queue_name, patch):
        _log.info("Releasing work item %s from %s" % (patch.id(), queue_name))
        return NetworkTransaction(convert_404_to_None=True).run(lambda: self._post_release_work_item(queue_name, patch))

    def _post_release_lock(self, queue_name, patch):
        release_lock_url = '{}/release-lock'.format(self._server_url())
        self._browser.open(release_lock_url)
        self._browser.select_form(name="release_lock")
        self._browser["queue_name"] = queue_name
        self._browser["attachment_id"] = unicode(patch.id())
        self._browser.submit()

    def release_lock(self, queue_name, patch):
        _log.info("Releasing lock for work item %s from %s" % (patch.id(), queue_name))
        return NetworkTransaction(convert_404_to_None=True).run(lambda: self._post_release_lock(queue_name, patch))

    def update_work_items(self, queue_name, high_priority_work_items, work_items):
        _log.info("Recording work items: %s for %s" % (high_priority_work_items + work_items, queue_name))
        return NetworkTransaction().run(lambda: self._post_work_items_to_server(queue_name, high_priority_work_items, work_items))

    def update_status(self, queue_name, status, patch=None, results_file=None):
        _log.info(status)
        return NetworkTransaction().run(lambda: self._post_status_to_server(queue_name, status, patch, results_file))

    def update_svn_revision(self, svn_revision_number, broken_bot):
        _log.info("SVN revision: %s broke %s" % (svn_revision_number, broken_bot))
        return NetworkTransaction().run(lambda: self._post_svn_revision_to_server(svn_revision_number, broken_bot))

    def _fetch_attachment_page(self, action, attachment_id):
        attachment_url = '{}/attachment/{}/{}'.format(self._server_url(), action, attachment_id)
        _log.info('Fetching: {}'.format(attachment_url))
        return self._fetch_url(attachment_url)

    def fetch_attachment(self, attachment_id):
        # We will neither have metadata nor content if our API key is missing, invalid, or revoked.
        attachment_metadata = self._fetch_attachment_page('metadata', attachment_id)
        if not attachment_metadata:
            return None
        attachment_contents = self._fetch_attachment_page('data', attachment_id)
        if not attachment_contents:
            return None
        attachment = Attachment.from_json(attachment_metadata)
        attachment.contents = lambda: attachment_contents
        return attachment

    def _fetch_url(self, url):
        # FIXME: This should use NetworkTransaction's 404 handling instead.
        try:
            request = urllib2.Request(url)
            if self._api_key:
                request.add_header(*self._authorization_header_name_and_value_pair())
            return urllib2.urlopen(request, timeout=300).read()
        except urllib2.HTTPError as e:
            if e.code == 404:
                return None
            raise e

    def patch_status(self, queue_name, patch_id):
        patch_status_url = '{}/patch-status/{}/{}'.format(self._server_url(), queue_name, patch_id)
        return self._fetch_url(patch_status_url)

    def svn_revision(self, svn_revision_number):
        svn_revision_url = '{}/svn-revision/{}'.format(self._server_url(), svn_revision_number)
        return self._fetch_url(svn_revision_url)

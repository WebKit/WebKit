# Copyright (C) 2018 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import base64
import logging
import os
import re
import socket
import time

from datetime import datetime, timedelta

from ews.models.patch import Patch
from ews.thirdparty.BeautifulSoup import BeautifulSoup, SoupStrainer
import ews.common.util as util
import ews.config as config

_log = logging.getLogger(__name__)


class Bugzilla():
    @classmethod
    def retrieve_attachment(cls, attachment_id):
        attachment_json = Bugzilla._fetch_attachment_json(attachment_id)
        if not attachment_json or not attachment_json.get('data'):
            _log.warn('Unable to fetch attachment "{}".'.format(attachment_id))
            return None

        attachment_data = base64.b64decode(attachment_json.get('data'))
        Bugzilla.save_attachment(attachment_id, attachment_data)
        attachment_json['path'] = Bugzilla.file_path_for_patch(attachment_id)
        return attachment_json

    @classmethod
    def save_attachment(cls, attachment_id, attachment_data):
        with open(Bugzilla.file_path_for_patch(attachment_id), 'w') as attachment_file:
            attachment_file.write(attachment_data)

    @classmethod
    def _fetch_attachment_json(cls, attachment_id):
        if not Patch.is_valid_patch_id(attachment_id):
            _log.warn('Invalid attachment id: "{}", skipping download.'.format(attachment_id))
            return None

        attachment_url = '{}rest/bug/attachment/{}'.format(config.BUG_SERVER_URL, attachment_id)
        api_key = os.getenv('BUGZILLA_API_KEY', None)
        if api_key:
            attachment_url += '?api_key={}'.format(api_key)
        attachment = util.fetch_data_from_url(attachment_url)
        if not attachment:
            return None
        attachment_json = attachment.json().get('attachments')
        if not attachment_json or len(attachment_json) == 0:
            return None
        return attachment_json.get(str(attachment_id))

    @classmethod
    def file_path_for_patch(cls, patch_id):
        if not os.path.exists(config.PATCH_FOLDER):
            os.mkdir(config.PATCH_FOLDER)
        return config.PATCH_FOLDER + '{}.patch'.format(patch_id)

    @classmethod
    def get_list_of_patches_needing_reviews(cls):
        current_time = datetime.today()
        ids_needing_review = set(BugzillaBeautifulSoup().fetch_attachment_ids_from_review_queue(current_time - timedelta(7)))
        #TODO: add security bugs support here.
        return ids_needing_review


class BugzillaBeautifulSoup():
    # FIXME: Deprecate this class when https://bugzilla.mozilla.org/show_bug.cgi?id=1508531 is fixed.
    def __init__(self):
        self._browser = None

    def _get_browser(self):
        if not self._browser:
            socket.setdefaulttimeout(600)
            from mechanize import Browser
            self._browser = Browser()
            self._browser.set_handle_robots(False)
        return self._browser

    def _set_browser(self, value):
        self._browser = value

    browser = property(_get_browser, _set_browser)

    def authenticate(self):
        username = os.getenv('BUGZILLA_USERNAME', None)
        password = os.getenv('BUGZILLA_PASSWORD', None)
        if not username or not password:
            _log.warn('Bugzilla username/password not configured in environment variables. Skipping authentication.')
            return

        authenticated = False
        attempts = 0
        while not authenticated:
            attempts += 1
            _log.info('Logging in as {}...'.format(username))
            self.browser.open(config.BUG_SERVER_URL + 'index.cgi?GoAheadAndLogIn=1')
            self.browser.select_form(name="login")
            self.browser['Bugzilla_login'] = username
            self.browser['Bugzilla_password'] = password
            self.browser.find_control("Bugzilla_restrictlogin").items[0].selected = False
            response = self.browser.submit()

            match = re.search("<title>(.+?)</title>", response.read())
            # If the resulting page has a title, and it contains the word
            # "invalid" assume it's the login failure page.
            if match and re.search("Invalid", match.group(1), re.IGNORECASE):
                errorMessage = 'Bugzilla login failed: {}'.format(match.group(1))
                if attempts >= 5:
                    # raise an exception only if this was the last attempt
                    raise Exception(errorMessage)
                _log.error(errorMessage)
                time.sleep(5)
            else:
                authenticated = True

    def fetch_attachment_ids_from_review_queue(self, since=None, only_security_bugs=False):
        review_queue_url = 'request.cgi?action=queue&type=review&group=type'
        if only_security_bugs:
            review_queue_url += '&product=Security'
        return self._parse_attachment_ids_request_query(self._load_query(review_queue_url), since)

    def _load_query(self, query):
        self.authenticate()
        full_url = '{}{}'.format(config.BUG_SERVER_URL, query)
        _log.info('Getting list of patches needing review, URL: {}'.format(full_url))
        return self.browser.open(full_url)

    def _parse_attachment_ids_request_query(self, page, since=None):
        # Formats
        digits = re.compile("\d+")
        attachment_href = re.compile("attachment.cgi\?id=\d+&action=review")
        # if no date is given, return all ids
        if not since:
            attachment_links = SoupStrainer("a", href=attachment_href)
            return [int(digits.search(tag["href"]).group(0))
                for tag in BeautifulSoup(page, parseOnlyThese=attachment_links)]

        # Parse the main table only
        date_format = re.compile("\d{4}-\d{2}-\d{2} \d{2}:\d{2}")
        mtab = SoupStrainer("table", {"class": "requests"})
        soup = BeautifulSoup(page, parseOnlyThese=mtab)
        patch_ids = []

        for row in soup.findAll("tr"):
            patch_tag = row.find("a", {"href": attachment_href})
            if not patch_tag:
                continue
            patch_id = int(digits.search(patch_tag["href"]).group(0))
            date_tag = row.find("td", text=date_format)
            if date_tag and datetime.strptime(date_format.search(date_tag).group(0), "%Y-%m-%d %H:%M") < since:
                continue
            patch_ids.append(patch_id)
        return patch_ids

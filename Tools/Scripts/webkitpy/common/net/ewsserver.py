# Copyright (C) 2019 Apple Inc. All rights reserved.
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

from webkitcorepy import unicode

from webkitpy.common.config.urls import ewsserver_default_host
from webkitpy.common.net.networktransaction import NetworkTransaction

import logging

_log = logging.getLogger(__name__)


class EWSServer:
    def __init__(self, host=ewsserver_default_host, use_https=True, browser=None):
        self.host = host
        self.use_https = bool(use_https)
        from webkitpy.thirdparty.autoinstalled.mechanize import Browser
        self._browser = browser or Browser()
        self._browser.set_handle_robots(False)

    def _server_url(self):
        return '{}://{}'.format(('https' if self.use_https else 'http'), self.host)

    def _post_patch_to_ews(self, attachment_id):
        submit_to_ews_url = '{}/submit-to-ews'.format(self._server_url())
        self._browser.open(submit_to_ews_url)
        self._browser.select_form(name='submit_to_ews')
        self._browser['patch_id'] = unicode(attachment_id)
        self._browser.submit()

    def submit_to_ews(self, attachment_id):
        _log.info('Submitting attachment {} to EWS queues'.format(attachment_id))
        return NetworkTransaction().run(lambda: self._post_patch_to_ews(attachment_id))

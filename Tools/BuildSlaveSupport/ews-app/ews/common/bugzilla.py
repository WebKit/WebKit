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

from ews.models.patch import Patch
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

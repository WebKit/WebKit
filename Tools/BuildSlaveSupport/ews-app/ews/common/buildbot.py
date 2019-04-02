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

from future.utils import lrange

import logging
import os
import re
import subprocess

import ews.common.util as util
import ews.config as config

_log = logging.getLogger(__name__)


class Buildbot():
    # Buildbot status codes referenced from https://github.com/buildbot/buildbot/blob/master/master/buildbot/process/results.py
    ALL_RESULTS = lrange(7)
    SUCCESS, WARNINGS, FAILURE, SKIPPED, EXCEPTION, RETRY, CANCELLED = ALL_RESULTS

    @classmethod
    def send_patch_to_buildbot(cls, patch_path, properties=[]):
        command = ['buildbot', 'try',
                   '--connect=pb',
                   '--master={}:{}'.format(config.BUILDBOT_SERVER_HOST, config.BUILDBOT_SERVER_PORT),
                   '--username={}'.format(config.BUILDBOT_TRY_USERNAME),
                   '--passwd={}'.format(config.BUILDBOT_TRY_PASSWORD),
                   '--diff={}'.format(patch_path),
                   '--repository=']

        for property in properties:
            command.append('--property={}'.format(property))

        _log.debug('Executing command: {}'.format(command))
        return_code = subprocess.call(command)
        if return_code:
            _log.warn('Error executing: {}, return code={}'.format(command, return_code))

        return return_code

    @classmethod
    def get_builder_id_to_name_mapping(cls):
        builder_id_to_name_mapping = {}
        builder_url = 'http://{}/api/v2/builders'.format(config.BUILDBOT_SERVER_HOST)
        builders_data = util.fetch_data_from_url(builder_url)
        if not builders_data:
            return {}
        for builder in builders_data.json().get('builders', []):
            builder_id = builder['builderid']
            builder_name = builder.get('name')
            display_name = builder.get('description')
            if not display_name:
                display_name = Buildbot._get_display_name_from_builder_name(builder_name)
            builder_id_to_name_mapping[builder_id] = {'builder_name': builder_name, 'display_name': display_name}
        return builder_id_to_name_mapping

    @classmethod
    def _get_display_name_from_builder_name(cls, builder_name):
        words = re.split('[, \-_:()]+', builder_name)
        if not words:
            return builder_name
        return words[0].lower()

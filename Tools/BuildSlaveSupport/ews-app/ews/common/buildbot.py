# Copyright (C) 2018-2020 Apple Inc. All rights reserved.
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
import requests
import subprocess

import ews.common.util as util
import ews.config as config

_log = logging.getLogger(__name__)


class Buildbot():
    # Buildbot status codes referenced from https://github.com/buildbot/buildbot/blob/master/master/buildbot/process/results.py
    ALL_RESULTS = lrange(7)
    SUCCESS, WARNINGS, FAILURE, SKIPPED, EXCEPTION, RETRY, CANCELLED = ALL_RESULTS
    icons_for_queues_mapping = {}
    queue_name_by_shortname_mapping = {}
    builder_name_to_id_mapping = {}

    @classmethod
    def send_patch_to_buildbot(cls, patch_path, send_to_commit_queue=False, properties=None):
        properties = properties or []
        buildbot_port = config.COMMIT_QUEUE_PORT if send_to_commit_queue else config.BUILDBOT_SERVER_PORT
        command = ['buildbot', 'try',
                   '--connect=pb',
                   '--master={}:{}'.format(config.BUILDBOT_SERVER_HOST, buildbot_port),
                   '--username={}'.format(config.BUILDBOT_TRY_USERNAME),
                   '--passwd={}'.format(config.BUILDBOT_TRY_PASSWORD),
                   '--diff={}'.format(patch_path),
                   '--repository=']

        for property in properties:
            command.append('--property={}'.format(property))

        return_code = subprocess.call(command)
        if return_code:
            _log.warn('Error executing buildbot try command for {}, return code={}'.format(patch_path, return_code))

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

    @classmethod
    def fetch_config(cls):
        config_url = 'https://{}/config.json'.format(config.BUILDBOT_SERVER_HOST)
        config_data = util.fetch_data_from_url(config_url)
        if not config_data:
            return {}
        return config_data.json()

    @classmethod
    def update_icons_for_queues_mapping(cls):
        config = cls.fetch_config()
        if not config:
            _log.warn('Unable to fetch buildbot config.json')
        for builder in config.get('builders', []):
            shortname = builder.get('shortname')
            Buildbot.icons_for_queues_mapping[shortname] = builder.get('icon')
            Buildbot.queue_name_by_shortname_mapping[shortname] = builder.get('name')

    @classmethod
    def update_builder_name_to_id_mapping(cls):
        url = 'https://{}/api/v2/builders'.format(config.BUILDBOT_SERVER_HOST)
        builders_data = util.fetch_data_from_url(url)
        if not builders_data:
            return
        for builder in builders_data.json().get('builders', []):
            name = builder.get('name')
            Buildbot.builder_name_to_id_mapping[name] = builder.get('builderid')

    @classmethod
    def fetch_pending_and_inprogress_builds(cls, builder_full_name):
        builderid = Buildbot.builder_name_to_id_mapping.get(builder_full_name)
        if not Buildbot.builder_name_to_id_mapping:
            _log.warn('Missing builder_name_to_id_mapping, refetching it from {}'.format(config.BUILDBOT_SERVER_HOST))
            cls.update_builder_name_to_id_mapping()

        if not builderid:
            _log.error('Invalid builder: {}. Number of builders: {}'.format(builder_full_name, len(cls.builder_name_to_id_mapping)))
            return {}
        url = 'https://{}/api/v2/builders/{}/buildrequests?complete=false&property=*'.format(config.BUILDBOT_SERVER_HOST, builderid)
        builders_data = util.fetch_data_from_url(url)
        if not builders_data:
            return {}
        return builders_data.json()

    @classmethod
    def get_patches_in_queue(cls, builder_full_name):
        patch_ids = []
        builds = cls.fetch_pending_and_inprogress_builds(builder_full_name)
        for buildrequest in builds.get('buildrequests', []):
            properties = buildrequest.get('properties')
            if properties:
                patch_ids.append(properties.get('patch_id')[0])
        _log.debug('Patches in queue for {}: {}'.format(builder_full_name, patch_ids))
        return patch_ids

    @classmethod
    def retry_build(cls, builder_id, build_number):
        if not (util.is_valid_id(builder_id) and util.is_valid_id(build_number)):
            return False

        build_url = 'https://{}/api/v2/builders/{}/builds/{}'.format(config.BUILDBOT_SERVER_HOST, builder_id, build_number)
        username = os.getenv('EWS_ADMIN_USERNAME')
        password = os.getenv('EWS_ADMIN_PASSWORD')
        session = requests.Session()
        response = session.head('https://{}/auth/login'.format(config.BUILDBOT_SERVER_HOST), auth=(username, password))
        if (not response) or response.status_code not in (200, 302):
            _log.error('Authentication to {} failed. Please check username/password.'.format(config.BUILDBOT_SERVER_HOST))
            return False

        json_data = {'method': 'rebuild', 'id': 1, 'jsonrpc': '2.0', 'params': {'reason': 'retried-by-user'}}
        response = session.post(build_url, json=json_data)

        if response and response.status_code == 200:
            _log.info('Successfuly submitted retry request for build: {}'.format(build_url))
            return True

        _log.error('Failed to retry build: {}, http response code: {}'.format(build_url, response.status_code))
        return False

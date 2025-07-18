# Copyright (C) 2018-2025 Apple Inc. All rights reserved.
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

from __future__ import unicode_literals

import json
import logging
import os

from django.http import HttpResponse
from django.shortcuts import render
from django.views import View
from django.views.decorators.csrf import csrf_exempt
from django.utils.decorators import method_decorator

from ews.config import SUCCESS
from ews.common.github import GitHubEWS
from ews.models.build import Build
from ews.models.step import Step
import ews.common.util as util

_log = logging.getLogger(__name__)


@method_decorator(csrf_exempt, name='dispatch')
class Results(View):
    def post(self, request):
        data = json.loads(request.body)

        if data.get('EWS_API_KEY') != util.load_password('EWS_API_KEY'):
            _log.error('Incorrect API Key {}. Host: {}. Ignoring data.'.format(data.get('EWS_API_KEY'), data.get('hostname')))
            return HttpResponse('Incorrect API Key received')

        if data.get('type') == u'ews-build':
            return self.build_event(data)

        if data.get('type') == u'ews-step':
            return self.step_event(data)

        _log.error('Unexpected data type received: {}'.format(data.get('type')))
        return HttpResponse('Unexpected data type received: {}'.format(data.get('type')))

    def build_event(self, data):
        if not self.is_valid_result(data):
            return HttpResponse('Incomplete data.')

        change_id = data['change_id']
        pr_number = data.get('pr_number', data.get('pr_id')) or -1
        pr_project = data.get('pr_project', '') or ''

        _log.info(f'Build {data["status"]}, change_id: {change_id}, build_id: {data["build_id"]}, pr_number: {pr_number}, pr_project: {pr_project}')
        if not change_id:
            _log.error('change_id missing: {}'.format(change_id))
            return HttpResponse('Invalid change id: {}.'.format(change_id))

        rc = Build.save_build(change_id=change_id, hostname=data['hostname'], build_id=data['build_id'], builder_id=data['builder_id'], builder_name=data['builder_name'],
                              builder_display_name=data['builder_display_name'], number=data['number'], result=data['result'],
                              state_string=data['state_string'], started_at=data['started_at'], complete_at=data['complete_at'], pr_number=pr_number, pr_project=pr_project)
        if rc == SUCCESS and pr_number and pr_number != -1:
            # For PR builds leave comment on PR
            allow_new_comment = (data['status'] == 'started' and data['builder_display_name'] in ['services', 'ios-wk2'])
            GitHubEWS.add_or_update_comment_for_change_id(change_id, pr_number, pr_project, allow_new_comment)
        return HttpResponse('Saved data for change: {}.\n'.format(change_id))

    def step_event(self, data):
        if not self.is_valid_result(data):
            _log.warn('Incomplete step event data')
            return HttpResponse('Incomplete data.')
        _log.info('Step event received, step-id: {}'.format(data['step_id']))

        Step.save_step(hostname=data['hostname'], step_id=data['step_id'], build_id=data['build_id'], result=data['result'],
                   state_string=data['state_string'], started_at=data['started_at'], complete_at=data['complete_at'])
        return HttpResponse('Saved data for step: {}.\n'.format(data['step_id']))

    def is_valid_result(self, data):
        if data['type'] not in [u'ews-build', u'ews-step']:
            _log.error('Invalid data type: {}'.format(data['type']))
            return False

        required_keys = {u'ews-build': ['hostname', 'change_id', 'build_id', 'builder_id', 'builder_name', 'builder_display_name',
                                           'number', 'result', 'state_string', 'started_at', 'complete_at'],
                         u'ews-step': ['hostname', 'step_id', 'build_id', 'result', 'state_string', 'started_at', 'complete_at']}

        for key in required_keys.get(data.get('type')):
            if key not in data:
                _log.warn('Data is missing {}'.format(key))
                return False
        return True

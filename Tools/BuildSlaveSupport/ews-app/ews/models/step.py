# Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

import logging

from django.db import models
from ews.config import ERR_UNEXPECTED, SUCCESS
from ews.models.build import Build
from ews.models.buildbotinstance import BuildbotInstance
import ews.common.util as util

_log = logging.getLogger(__name__)


class Step(models.Model):
    uid = models.TextField(primary_key=True)
    build_uid = models.ForeignKey(Build, on_delete=models.CASCADE, db_column='build_uid', db_constraint=False)
    result = models.IntegerField(null=True, blank=True)
    state_string = models.TextField()
    started_at = models.IntegerField(null=True, blank=True)
    complete_at = models.IntegerField(null=True, blank=True)
    created = models.DateTimeField(auto_now_add=True)
    modified = models.DateTimeField(auto_now=True)

    def __str__(self):
        return str(self.uid)

    @classmethod
    def save_step(cls, hostname, step_id, build_id, result, state_string, started_at, complete_at=None):
        if not Step.is_valid_result(step_id, build_id, result, state_string, started_at, complete_at):
            return ERR_UNEXPECTED

        if state_string is None:
            state_string = ''

        step_uid = BuildbotInstance.get_uid(hostname, step_id)
        build_uid = BuildbotInstance.get_uid(hostname, build_id)

        step = Step.get_existing_step(step_uid)
        if step:
            # If the step data is already present in database, update it, e.g.: step complete event.
            return Step.update_step(step, step_uid, build_uid, result, state_string, started_at, complete_at)

        # Save the new step data, e.g.: step start event.
        Step(step_uid, build_uid, result, state_string, started_at, complete_at).save()
        _log.info('Saved step {} in database for build: {}'.format(step_uid, build_uid))
        return SUCCESS

    @classmethod
    def update_step(cls, step, step_uid, build_uid, result, state_string, started_at, complete_at):
        if step.uid != step_uid:
            _log.error('step_uid {} does not match with step_uid {}. Ignoring new data.'.format(step.uid, step_uid))
            return ERR_UNEXPECTED
        step.result = result
        step.state_string = state_string
        step.started_at = started_at
        step.complete_at = complete_at
        step.save(update_fields=['result', 'state_string', 'started_at', 'complete_at', 'modified'])
        _log.info('Updated step {} in database for build: {}'.format(step_uid, build_uid))
        return SUCCESS

    @classmethod
    def get_existing_step(cls, uid):
        try:
            return Step.objects.get(uid=uid)
        except Step.DoesNotExist:
            return None

    @classmethod
    def is_valid_result(cls, step_id, build_id, result, state_string, started_at, complete_at):
        if not (util.is_valid_id(step_id) and util.is_valid_id(build_id)):
            return False

        return True

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
from ews.models.buildbotinstance import BuildbotInstance
from ews.models.patch import Patch
import ews.common.util as util

_log = logging.getLogger(__name__)


class Build(models.Model):
    patch = models.ForeignKey(Patch, on_delete=models.CASCADE, db_constraint=False)
    uid = models.TextField(primary_key=True)
    builder_id = models.IntegerField()
    builder_name = models.TextField()
    builder_display_name = models.TextField()
    number = models.IntegerField()
    result = models.IntegerField(null=True, blank=True)
    state_string = models.TextField()
    started_at = models.IntegerField(null=True, blank=True)
    complete_at = models.IntegerField(null=True, blank=True)
    retried = models.BooleanField(default=False)
    created = models.DateTimeField(auto_now_add=True)
    modified = models.DateTimeField(auto_now=True)

    def __str__(self):
        return str(self.uid)

    @classmethod
    def save_build(cls, patch_id, hostname, build_id, builder_id, builder_name, builder_display_name, number, result, state_string, started_at, complete_at=None):
        if not Build.is_valid_result(patch_id, build_id, builder_id, number, result, state_string, started_at, complete_at):
            return ERR_UNEXPECTED

        if state_string is None:
            state_string = ''

        uid = BuildbotInstance.get_uid(hostname, build_id)
        build = Build.get_existing_build(uid)
        if build:
            # If the build data is already present in database, update it, e.g.: build complete event.
            return Build.update_build(build, patch_id, uid, builder_id, builder_name, builder_display_name, number, result, state_string, started_at, complete_at)

        # Save the new build data, e.g.: build start event.
        Build(patch_id, uid, builder_id, builder_name, builder_display_name, number, result, state_string, started_at, complete_at).save()
        _log.info('Saved build {} in database for patch_id: {}'.format(uid, patch_id))
        return SUCCESS

    @classmethod
    def update_build(cls, build, patch_id, uid, builder_id, builder_name, builder_display_name, number, result, state_string, started_at, complete_at):
        if build.patch_id != patch_id:
            _log.error('patch_id {} does not match with patch_id {}. Ignoring new data.'.format(build.patch_id, patch_id))
            return ERR_UNEXPECTED
        if build.uid != uid:
            _log.error('uid {} does not match with uid {}. Ignoring new data.'.format(build.uid, uid))
            return ERR_UNEXPECTED
        if build.builder_id != builder_id:
            _log.error('builder_id {} does not match with builder_id {}. Ignoring new data.'.format(build.builder_id, builder_id))
            return ERR_UNEXPECTED
        if build.number != number:
            _log.error('build number {} does not match with number {}. Ignoring new data.'.format(build.number, number))
            return ERR_UNEXPECTED
        build.result = result
        build.state_string = state_string
        build.started_at = started_at
        build.complete_at = complete_at
        build.save(update_fields=['result', 'state_string', 'started_at', 'complete_at', 'modified'])
        _log.debug('Updated build {} in database for patch_id: {}'.format(uid, patch_id))
        return SUCCESS

    @classmethod
    def set_retried(cls, uid, retried=True):
        build = Build.get_existing_build(uid)
        if not build:
            return
        build.retried = retried
        build.save(update_fields=['retried', 'modified'])
        _log.info('Updated build {} in database with retried={}'.format(uid, retried))

    @classmethod
    def get_existing_build(cls, uid):
        try:
            return Build.objects.get(uid=uid)
        except Build.DoesNotExist:
            return None

    @classmethod
    def is_valid_result(cls, patch_id, build_id, builder_id, number, result, state_string, started_at, complete_at=None):
        if not (util.is_valid_id(patch_id) and util.is_valid_id(build_id) and util.is_valid_id(builder_id) and util.is_valid_id(number)):
            return False

        return True

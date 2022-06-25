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

from __future__ import unicode_literals

import logging

from django.db import models

from ews.config import ERR_EXISTING_PATCH, ERR_INVALID_PATCH_ID, ERR_NON_EXISTING_PATCH, SUCCESS

_log = logging.getLogger(__name__)


class Patch(models.Model):
    patch_id = models.IntegerField(primary_key=True)
    bug_id = models.IntegerField()
    obsolete = models.BooleanField(default=False)
    sent_to_buildbot = models.BooleanField(default=False)
    sent_to_commit_queue = models.BooleanField(default=False)
    created = models.DateTimeField(auto_now_add=True)
    modified = models.DateTimeField(auto_now=True)

    def __str__(self):
        return str(self.patch_id)

    @classmethod
    def save_patch(cls, patch_id, bug_id=-1, obsolete=False, sent_to_buildbot=False, sent_to_commit_queue=False):
        if not Patch.is_valid_patch_id(patch_id):
            _log.warn('Patch id {} in invalid. Skipped saving.'.format(patch_id))
            return ERR_INVALID_PATCH_ID

        if Patch.is_existing_patch_id(patch_id):
            _log.debug('Patch id {} already exists in database. Skipped saving.'.format(patch_id))
            return ERR_EXISTING_PATCH
        Patch(patch_id, bug_id, obsolete, sent_to_buildbot, sent_to_commit_queue).save()
        _log.info('Saved patch in database, id: {}'.format(patch_id))
        return SUCCESS

    @classmethod
    def save_patches(cls, patch_id_list):
        for patch_id in patch_id_list:
            Patch.save_patch(patch_id)

    @classmethod
    def is_valid_patch_id(cls, patch_id):
        if not patch_id or patch_id < 1:
            _log.warn('Invalid patch id: {}'.format(patch_id))
            return False
        if type(patch_id) != int:
            _log.warn('Data type mismatch for patch_id, expected: int, found: {}, id: {}'.format(type(patch_id), patch_id))
            return False
        return True

    @classmethod
    def is_existing_patch_id(cls, patch_id):
        return bool(Patch.objects.filter(patch_id=patch_id))

    @classmethod
    def is_patch_sent_to_buildbot(cls, patch_id, commit_queue=False):
        if commit_queue:
            return Patch._is_patch_sent_to_commit_queue(patch_id)
        return Patch._is_patch_sent_to_buildbot(patch_id)

    @classmethod
    def _is_patch_sent_to_buildbot(cls, patch_id):
        return Patch.is_existing_patch_id(patch_id) and Patch.objects.get(pk=patch_id).sent_to_buildbot

    @classmethod
    def _is_patch_sent_to_commit_queue(cls, patch_id):
        return Patch.is_existing_patch_id(patch_id) and Patch.objects.get(pk=patch_id).sent_to_commit_queue

    @classmethod
    def get_patch(cls, patch_id):
        try:
            return Patch.objects.get(patch_id=patch_id)
        except:
            return None

    @classmethod
    def set_sent_to_buildbot(cls, patch_id, value, commit_queue=False):
        if commit_queue:
            return Patch._set_sent_to_commit_queue(patch_id, sent_to_commit_queue=value)
        return Patch._set_sent_to_buildbot(patch_id, sent_to_buildbot=value)

    @classmethod
    def _set_sent_to_buildbot(cls, patch_id, sent_to_buildbot=True):
        if not Patch.is_existing_patch_id(patch_id):
            Patch.save_patch(patch_id=patch_id, sent_to_buildbot=sent_to_buildbot)
            _log.info('Patch {} saved to database with sent_to_buildbot={}'.format(patch_id, sent_to_buildbot))
            return SUCCESS

        patch = Patch.objects.get(pk=patch_id)
        if patch.sent_to_buildbot == sent_to_buildbot:
            _log.warn('Patch {} already has sent_to_buildbot={}'.format(patch_id, sent_to_buildbot))
            return SUCCESS

        patch.sent_to_buildbot = sent_to_buildbot
        patch.save()
        _log.info('Updated patch {} with sent_to_buildbot={}'.format(patch_id, sent_to_buildbot))
        return SUCCESS

    @classmethod
    def _set_sent_to_commit_queue(cls, patch_id, sent_to_commit_queue=True):
        if not Patch.is_existing_patch_id(patch_id):
            Patch.save_patch(patch_id=patch_id, sent_to_commit_queue=sent_to_commit_queue)
            _log.info('Patch {} saved to database with sent_to_commit_queue={}'.format(patch_id, sent_to_commit_queue))
            return SUCCESS

        patch = Patch.objects.get(pk=patch_id)
        if patch.sent_to_commit_queue == sent_to_commit_queue:
            _log.warn('Patch {} already has sent_to_commit_queue={}'.format(patch_id, sent_to_commit_queue))
            return SUCCESS

        patch.sent_to_commit_queue = sent_to_commit_queue
        patch.save()
        _log.info('Updated patch {} with sent_to_commit_queue={}'.format(patch_id, sent_to_commit_queue))
        return SUCCESS

    @classmethod
    def set_bug_id(cls, patch_id, bug_id):
        if not Patch.is_existing_patch_id(patch_id):
            return ERR_NON_EXISTING_PATCH

        patch = Patch.objects.get(pk=patch_id)
        if patch.bug_id == bug_id:
            _log.warn('Patch {} already has bug id {} set.'.format(patch_id, bug_id))
            return SUCCESS

        patch.bug_id = bug_id
        patch.save()
        _log.debug('Updated patch {} with bug id {}'.format(patch_id, bug_id))
        return SUCCESS

    @classmethod
    def set_obsolete(cls, patch_id, obsolete=True):
        if not Patch.is_existing_patch_id(patch_id):
            return ERR_NON_EXISTING_PATCH

        patch = Patch.objects.get(pk=patch_id)
        if patch.obsolete == obsolete:
            _log.warn('Patch {} is already marked with obsolete={}.'.format(patch_id, obsolete))
            return SUCCESS
        patch.obsolete = obsolete
        patch.save()
        _log.debug('Marked patch {} as obsolete={}'.format(patch_id, obsolete))
        return SUCCESS

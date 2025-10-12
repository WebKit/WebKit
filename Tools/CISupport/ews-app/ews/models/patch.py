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

import logging

from django.db import models

from ews.config import ERR_EXISTING_CHANGE, ERR_INVALID_CHANGE_ID, ERR_NON_EXISTING_CHANGE, SUCCESS

_log = logging.getLogger(__name__)


class Change(models.Model):
    change_id = models.TextField(primary_key=True)
    bug_id = models.IntegerField()
    pr_number = models.IntegerField(default=-1)
    pr_project = models.TextField(default='')
    comment_id = models.BigIntegerField(default=-1)
    obsolete = models.BooleanField(default=False)
    sent_to_buildbot = models.BooleanField(default=False)
    sent_to_commit_queue = models.BooleanField(default=False)
    created = models.DateTimeField(auto_now_add=True)
    modified = models.DateTimeField(auto_now=True)

    def __str__(self):
        return str(self.change_id)

    @classmethod
    def save_change(cls, change_id, bug_id=-1, pr_number=-1, pr_project='', obsolete=False, sent_to_buildbot=False, sent_to_commit_queue=False):
        if not Change.is_valid_change_id(change_id):
            _log.warn('Change id {} in invalid. Skipped saving.'.format(change_id))
            return ERR_INVALID_CHANGE_ID

        if Change.is_existing_change_id(change_id):
            _log.debug('Change id {} already exists in database. Skipped saving.'.format(change_id))
            return ERR_EXISTING_CHANGE
        Change(change_id=change_id, bug_id=bug_id, pr_number=pr_number, pr_project=pr_project, obsolete=obsolete, sent_to_buildbot=sent_to_buildbot, sent_to_commit_queue=sent_to_commit_queue).save()
        _log.info(f'Saved change in database, id: {change_id}, pr_number: {pr_number}, pr_project: {pr_project}')
        return SUCCESS

    @classmethod
    def save_changes(cls, change_id_list):
        for change_id in change_id_list:
            Change.save_change(change_id)

    @classmethod
    def is_valid_change_id(cls, change_id):
        if not change_id:
            _log.warn('Invalid change id: {}'.format(change_id))
            return False
        return True

    @classmethod
    def is_existing_change_id(cls, change_id):
        return bool(Change.objects.filter(change_id=change_id))

    @classmethod
    def is_change_sent_to_buildbot(cls, change_id, commit_queue=False):
        if commit_queue:
            return Change._is_change_sent_to_commit_queue(change_id)
        return Change._is_change_sent_to_buildbot(change_id)

    @classmethod
    def _is_change_sent_to_buildbot(cls, change_id):
        return Change.is_existing_change_id(change_id) and Change.objects.get(pk=change_id).sent_to_buildbot

    @classmethod
    def _is_change_sent_to_commit_queue(cls, change_id):
        return Change.is_existing_change_id(change_id) and Change.objects.get(pk=change_id).sent_to_commit_queue

    @classmethod
    def get_change(cls, change_id):
        try:
            return Change.objects.get(change_id=change_id)
        except:
            return None

    @classmethod
    def mark_old_changes_as_obsolete(cls, pr_number, change_id):
        changes = Change.objects.filter(pr_number=pr_number).order_by('-created')
        if not changes or len(changes) == 1:
            return []
        obsolete_changes = []
        for change in changes[1:]:
            if not change.obsolete:
                if change.change_id == change_id:
                    _log.info(f'Marking change {change_id} on pr {pr_number} as obsolete, even though we just received builds for it. Latest commit:{change[0].pr_number}')
                change.obsolete = True
                change.save()
                obsolete_changes.append(change)
                _log.info(f'Marked change {change.change_id} on pr {pr_number} as obsolete')
        return obsolete_changes

    @classmethod
    def set_sent_to_buildbot(cls, change_id, value, commit_queue=False):
        if commit_queue:
            return Change._set_sent_to_commit_queue(change_id, sent_to_commit_queue=value)
        return Change._set_sent_to_buildbot(change_id, sent_to_buildbot=value)

    @classmethod
    def _set_sent_to_buildbot(cls, change_id, sent_to_buildbot=True):
        if not Change.is_existing_change_id(change_id):
            Change.save_change(change_id=change_id, sent_to_buildbot=sent_to_buildbot)
            _log.info('Change {} saved to database with sent_to_buildbot={}'.format(change_id, sent_to_buildbot))
            return SUCCESS

        change = Change.objects.get(pk=change_id)
        if change.sent_to_buildbot == sent_to_buildbot:
            _log.warn('Change {} already has sent_to_buildbot={}'.format(change_id, sent_to_buildbot))
            return SUCCESS

        change.sent_to_buildbot = sent_to_buildbot
        change.save()
        _log.info('Updated change {} with sent_to_buildbot={}'.format(change_id, sent_to_buildbot))
        return SUCCESS

    @classmethod
    def _set_sent_to_commit_queue(cls, change_id, sent_to_commit_queue=True):
        if not Change.is_existing_change_id(change_id):
            Change.save_change(change_id=change_id, sent_to_commit_queue=sent_to_commit_queue)
            _log.info('Change {} saved to database with sent_to_commit_queue={}'.format(change_id, sent_to_commit_queue))
            return SUCCESS

        change = Change.objects.get(pk=change_id)
        if change.sent_to_commit_queue == sent_to_commit_queue:
            _log.warn('Change {} already has sent_to_commit_queue={}'.format(change_id, sent_to_commit_queue))
            return SUCCESS

        change.sent_to_commit_queue = sent_to_commit_queue
        change.save()
        _log.info('Updated change {} with sent_to_commit_queue={}'.format(change_id, sent_to_commit_queue))
        return SUCCESS

    @classmethod
    def set_bug_id(cls, change_id, bug_id):
        if not Change.is_existing_change_id(change_id):
            return ERR_NON_EXISTING_CHANGE

        change = Change.objects.get(pk=change_id)
        if change.bug_id == bug_id:
            _log.warn('Change {} already has bug id {} set.'.format(change_id, bug_id))
            return SUCCESS

        change.bug_id = bug_id
        change.save()
        _log.debug('Updated change {} with bug id {}'.format(change_id, bug_id))
        return SUCCESS

    @classmethod
    def set_obsolete(cls, change_id, obsolete=True):
        if not Change.is_existing_change_id(change_id):
            return ERR_NON_EXISTING_CHANGE

        change = Change.objects.get(pk=change_id)
        if change.obsolete == obsolete:
            _log.warn('Change {} is already marked with obsolete={}.'.format(change_id, obsolete))
            return SUCCESS
        change.obsolete = obsolete
        change.save()
        _log.debug('Marked change {} as obsolete={}'.format(change_id, obsolete))
        return SUCCESS

    def set_comment_id(self, comment_id):
        if type(comment_id) != int or comment_id < 0:
            _log.error('Invalid comment_id {}, while trying to set comment_id for change: {}'.format(comment_id, self.change_id))
            return FAILURE

        if self.comment_id == comment_id:
            _log.warn('Change {} already has comment id {} set.'.format(self.change_id, comment_id))
            return SUCCESS

        self.comment_id = comment_id
        self.save()
        _log.info('Updated change {} with comment id {}'.format(self.change_id, comment_id))
        return SUCCESS

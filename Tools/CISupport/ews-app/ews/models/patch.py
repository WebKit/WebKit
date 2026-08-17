# Copyright (C) 2018-2026 Apple Inc. All rights reserved.
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

from ews.config import ERR_EXISTING_CHANGE, ERR_INVALID_CHANGE_ID, SUCCESS

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
    def save_change(cls, change_id, bug_id=-1, pr_number=-1, pr_project='', obsolete=False):
        if not Change.is_valid_change_id(change_id):
            _log.warn('Change id {} in invalid. Skipped saving.'.format(change_id))
            return ERR_INVALID_CHANGE_ID

        if Change.is_existing_change_id(change_id):
            _log.debug('Change id {} already exists in database. Skipped saving.'.format(change_id))
            return ERR_EXISTING_CHANGE
        Change(change_id=change_id, bug_id=bug_id, pr_number=pr_number, pr_project=pr_project, obsolete=obsolete).save()
        _log.info(f'Saved change in database, id: {change_id}, pr_number: {pr_number}, pr_project: {pr_project}')
        return SUCCESS

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

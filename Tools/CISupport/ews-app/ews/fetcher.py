# Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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

import datetime
import logging
import pytz
import threading
import time
import traceback

from ews.common.bugzilla import Bugzilla
from ews.common.buildbot import Buildbot
from ews.config import ERR_BUG_CLOSED, ERR_OBSOLETE_CHANGE, ERR_UNABLE_TO_FETCH_CHANGE
from ews.models.patch import Change
from ews.views.statusbubble import StatusBubble

_log = logging.getLogger(__name__)


class FetchLoop():
    def __init__(self, interval=30):
        self.interval = interval
        thread = threading.Thread(target=self.run, args=())
        thread.daemon = True
        thread.start()

    def run(self):
        Buildbot.update_builder_name_to_id_mapping()
        while True:
            Buildbot.update_icons_for_queues_mapping()
            try:
                BugzillaPatchFetcher().fetch()
                BugzillaPatchFetcher().fetch_commit_queue_patches()
            except Exception as e:
                _log.error('Exception in BugzillaPatchFetcher: {}\n{}'.format(e, traceback.format_exc()))
            time.sleep(self.interval)


class BugzillaPatchFetcher():
    def fetch(self, patch_ids=None):
        if patch_ids and type(patch_ids) != list:
            _log.error('Error: patch_ids should be a list, found: {}'.format(type(patch_ids)))
            return -1

        if not patch_ids:
            patch_ids = Bugzilla.get_list_of_patches_needing_reviews()
        patch_ids = BugzillaPatchFetcher.filter_valid_patches(patch_ids)
        _log.debug('r? patches: {}'.format(patch_ids))
        Change.save_changes(patch_ids)
        patches_to_send = self.patches_to_send_to_buildbot(patch_ids)
        _log.info('{} r? patches, {} patches need to be sent to Buildbot: {}'.format(len(patch_ids), len(patches_to_send), patches_to_send))
        return self.send_patches_to_buildbot(patches_to_send)

    def fetch_commit_queue_patches(self):
        patch_ids_commit_queue = Bugzilla.get_list_of_patches_for_commit_queue()
        patch_ids_commit_queue = BugzillaPatchFetcher.filter_valid_patches(patch_ids_commit_queue)
        _log.debug('cq+ patches: {}'.format(patch_ids_commit_queue))
        Change.save_changes(patch_ids_commit_queue)
        patches_to_send = self.patches_to_send_to_commit_queue(patch_ids_commit_queue)
        _log.info('{} cq+ patches, {} patches need to be sent to commit queue: {}'.format(len(patch_ids_commit_queue), len(patches_to_send), patches_to_send))
        self.send_patches_to_buildbot(patches_to_send, send_to_commit_queue=True)

    def send_patches_to_buildbot(self, patches_to_send, send_to_commit_queue=False):
        if not patches_to_send:
            return
        for patch_id in patches_to_send:
            bz_patch = Bugzilla.retrieve_attachment(patch_id)
            if not bz_patch or bz_patch['id'] != patch_id:
                _log.error('Unable to retrive patch "{}"'.format(patch_id))
                if len(patches_to_send) == 1:
                    return ERR_UNABLE_TO_FETCH_CHANGE
                continue
            if bz_patch.get('is_obsolete'):
                _log.warn('Patch {} is obsolete, skipping'.format(patch_id))
                Change.set_obsolete(patch_id)
                if len(patches_to_send) == 1:
                    return ERR_OBSOLETE_CHANGE
                continue
            if Bugzilla.is_bug_closed(bz_patch['bug_id']) == 1:
                _log.warn('Bug {} for patch {} is already closed, skipping'.format(bz_patch['bug_id'], patch_id))
                if len(patches_to_send) == 1:
                    return ERR_BUG_CLOSED
                continue
            if not send_to_commit_queue and Change.is_change_sent_to_buildbot(patch_id):
                _log.error('Patch {} is already sent to buildbot.'.format(patch_id))
                continue
            Change.set_sent_to_buildbot(patch_id, True, commit_queue=send_to_commit_queue)
            rc = Buildbot.send_patch_to_buildbot(bz_patch['path'],
                     send_to_commit_queue=send_to_commit_queue,
                     properties=['patch_id={}'.format(patch_id), 'bug_id={}'.format(bz_patch['bug_id']), 'owner={}'.format(bz_patch.get('creator', ''))])
            if rc == 0:
                Change.set_bug_id(patch_id, bz_patch['bug_id'])
            else:
                _log.error('Failed to send patch to buildbot.')
                Change.set_sent_to_buildbot(patch_id, False, commit_queue=send_to_commit_queue)
                #FIXME: send an email for this failure

    def patches_to_send_to_buildbot(self, patch_ids):
        return [patch_id for patch_id in patch_ids if not Change.is_change_sent_to_buildbot(patch_id)]

    def patches_to_send_to_commit_queue(self, patch_ids):
        if not patch_ids:
            return patch_ids
        patches_in_queue = set(Buildbot.get_patches_in_queue('Commit-Queue'))
        patch_ids = [patch_id for patch_id in set(patch_ids) if str(patch_id) not in patches_in_queue]

        patch_ids_to_send = []
        for patch_id in patch_ids:
            patch = Change.get_change(patch_id)
            recent_build, _ = StatusBubble().get_latest_build_for_queue(patch, 'commit')
            if not recent_build:
                patch_ids_to_send.append(patch_id)
                continue
            recent_build_timestamp = datetime.datetime.fromtimestamp(recent_build.complete_at, tz=pytz.UTC)
            cq_timestamp = Bugzilla.get_cq_plus_timestamp(patch_id)
            if not cq_timestamp:
                patch_ids_to_send.append(patch_id)
                continue
            if cq_timestamp > recent_build_timestamp:
                patch_ids_to_send.append(patch_id)
        return patch_ids_to_send

    @classmethod
    def filter_valid_patches(cls, patch_ids):
        return [p for p in patch_ids if Change.is_valid_change_id(p)]

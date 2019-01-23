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

import logging
import threading
import time

from ews.common.bugzilla import Bugzilla
from ews.common.buildbot import Buildbot
from ews.models.patch import Patch

_log = logging.getLogger(__name__)


class FetchLoop():
    def __init__(self, interval=60):
        self.interval = interval
        thread = threading.Thread(target=self.run, args=())
        thread.daemon = True
        thread.start()

    def run(self):
        while True:
            try:
                BugzillaPatchFetcher().fetch()
            except Exception as e:
                _log.error('Exception in BugzillaPatchFetcher: {}'.format(e))
            time.sleep(self.interval)


class BugzillaPatchFetcher():
    def fetch(self):
        patch_ids = Bugzilla.get_list_of_patches_needing_reviews()
        patch_ids = BugzillaPatchFetcher.filter_valid_patches(patch_ids)
        _log.debug('r? patches: {}'.format(patch_ids))
        Patch.save_patches(patch_ids)
        patches_to_send = self.patches_to_send_to_buildbot(patch_ids)
        _log.info('{} r? patches, {} patches need to be sent to Buildbot.'.format(len(patch_ids), len(patches_to_send)))

        for patch_id in patches_to_send:
            bz_patch = Bugzilla.retrieve_attachment(patch_id)
            if not bz_patch or bz_patch['id'] != patch_id:
                _log.error('Unable to retrive patch "{}"'.format(patch_id))
                continue
            if bz_patch.get('is_obsolete'):
                _log.warn('Patch is obsolete, skipping')
                Patch.set_obsolete(patch_id)
                continue
            rc = Buildbot.send_patch_to_buildbot(bz_patch['path'],
                     properties=['patch_id={}'.format(patch_id), 'bug_id={}'.format(bz_patch['bug_id']), 'owner={}'.format(bz_patch.get('creator', ''))])
            if rc == 0:
                Patch.set_bug_id(patch_id, bz_patch['bug_id'])
                Patch.set_sent_to_buildbot(patch_id)
            else:
                _log.error('Failed to send patch to buildbot.')
                #FIXME: send an email for this failure
        return patch_ids

    def patches_to_send_to_buildbot(self, patch_ids):
        return [patch_id for patch_id in patch_ids if not Patch.is_patch_sent_to_buildbot(patch_id)]

    @classmethod
    def filter_valid_patches(cls, patch_ids):
        return list(filter(lambda p: Patch.is_valid_patch_id(p), patch_ids))

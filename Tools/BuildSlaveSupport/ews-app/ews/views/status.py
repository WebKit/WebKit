# Copyright (C) 2020 Apple Inc. All rights reserved.
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

from django.http import JsonResponse
from django.views import View
from ews.common.buildbot import Buildbot
from ews.models.patch import Patch
from ews.views.statusbubble import StatusBubble
import ews.config as config


class Status(View):
    def _build_status(self, patch, queue):
        build, _ = StatusBubble().get_latest_build_for_queue(patch, queue)
        if not build:
            return {}

        return {
            'state': build.result,
            'url': 'https://{}/#/builders/{}/builds/{}'.format(config.BUILDBOT_SERVER_HOST, build.builder_id, build.number),
            'timestamp': build.complete_at,
        }

    def _build_statuses_for_patch(self, patch):
        if not patch:
            return []

        statuses = {}
        if patch.sent_to_buildbot:
            for queue in StatusBubble.ALL_QUEUES:
                status = self._build_status(patch, queue)
                if status:
                    statuses[queue] = status

        if patch.sent_to_commit_queue:
            cq_status = self._build_status(patch, 'commit')
            if cq_status:
                statuses['commit'] = cq_status

        return statuses

    def get(self, request, patch_id):
        patch_id = int(patch_id)
        patch = Patch.get_patch(patch_id)
        response_data = self._build_statuses_for_patch(patch)
        return JsonResponse(response_data)

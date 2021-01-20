# Copyright (C) 2019 Apple Inc. All rights reserved.
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

from django.http import HttpResponse
from django.shortcuts import redirect, render
from django.views import View
from django.views.decorators.clickjacking import xframe_options_exempt

from ews.common.buildbot import Buildbot
from ews.models.build import Build
from ews.models.patch import Patch
from ews.views.statusbubble import StatusBubble

_log = logging.getLogger(__name__)


class RetryPatch(View):
    def get(self, request):
        return render(request, 'retry.html', {})

    @xframe_options_exempt
    def post(self, request):
        try:
            patch_id = request.POST.get('patch_id')
            patch_id = int(patch_id)
        except (ValueError, TypeError) as e:
            return HttpResponse('Invalid patch id: {}'.format(request.POST.get('patch_id')))

        builds_to_retry = StatusBubble().find_failed_builds_for_patch(patch_id)
        _log.info('Retrying patch: {}. Failed builds: {}'.format(patch_id, builds_to_retry))
        if not builds_to_retry:
            return HttpResponse('No recent failed build(s) found for patch {}.'.format(patch_id))

        failed_to_retry_builds = []
        for build in builds_to_retry:
            if build.retried:
                _log.warn('Build {} for patch {} is already retried.'.format(build.uid, patch_id))
                continue
            Build.set_retried(build.uid, True)
            if not Buildbot.retry_build(build.builder_id, build.number):
                failed_to_retry_builds.append(build)
                Build.set_retried(build.uid, False)

        if len(failed_to_retry_builds) > 0:
            message = 'Failed to retry {} build(s) for patch {}.'.format(len(failed_to_retry_builds), patch_id)
            message += ' Please contact admin@webkit.org if the problem persist.'
            _log.warn(message)
            return HttpResponse(message)
        return redirect('/status-bubble/{}'.format(patch_id))

# Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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

from django.http import HttpResponse
from django.shortcuts import redirect, render
from django.views import View
from django.views.decorators.clickjacking import xframe_options_exempt

from ews.config import ERR_BUG_CLOSED, ERR_OBSOLETE_CHANGE, ERR_UNABLE_TO_FETCH_CHANGE
from ews.fetcher import BugzillaPatchFetcher
from ews.models.patch import Change

_log = logging.getLogger(__name__)


class SubmitToEWS(View):
    def get(self, request):
        return render(request, 'submittoews.html', {})

    @xframe_options_exempt
    def post(self, request):
        try:
            change_id = request.POST.get('change_id')
            _log.info('SubmitToEWS::change: {}'.format(change_id))
            change_id = int(change_id)
        except:
            return HttpResponse('Invalid patch id provided, should be an integer. git hashes are not supported.')

        if change_id < 459500:
            return HttpResponse('Patch is too old. Skipping.')

        if change_id > 600000:
            return HttpResponse('patch id is too large. Skipping.')

        if Change.is_change_sent_to_buildbot(change_id):
            _log.info('SubmitToEWS::change {} already submitted'.format(change_id))
            if request.POST.get('next_action') == 'return_to_bubbles':
                return redirect('/status-bubble/{}'.format(change_id))
            return HttpResponse("Change {} already submitted. Please wait for status-bubbles.".format(change_id))

        rc = BugzillaPatchFetcher().fetch([change_id])
        if rc == ERR_UNABLE_TO_FETCH_CHANGE:
            return HttpResponse('Set r? on patch, EWS is currently unable to access patch {}.'.format(change_id))
        if rc == ERR_OBSOLETE_CHANGE:
            return HttpResponse('Obsolete Patch: {}, not submitting to EWS.'.format(change_id))
        if rc == ERR_BUG_CLOSED:
            return HttpResponse('Closed Bug for patch: {}, not submitting to EWS.'.format(change_id))

        if request.POST.get('next_action') == 'return_to_bubbles':
            return redirect('/status-bubble/{}'.format(change_id))
        return HttpResponse("Submitted patch {} to EWS.".format(change_id))

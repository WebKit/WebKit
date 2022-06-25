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

from ews.config import ERR_BUG_CLOSED, ERR_OBSOLETE_PATCH, ERR_UNABLE_TO_FETCH_PATCH
from ews.fetcher import BugzillaPatchFetcher
from ews.models.patch import Patch

_log = logging.getLogger(__name__)


class SubmitToEWS(View):
    def get(self, request):
        return render(request, 'submittoews.html', {})

    @xframe_options_exempt
    def post(self, request):
        try:
            patch_id = request.POST.get('patch_id')
            patch_id = int(patch_id)
        except:
            return HttpResponse('Invalid patch id provided, should be an integer.')

        _log.info('SubmitToEWS::patch: {}'.format(patch_id))
        if Patch.is_patch_sent_to_buildbot(patch_id):
            _log.info('SubmitToEWS::patch {} already submitted'.format(patch_id))
            if request.POST.get('next_action') == 'return_to_bubbles':
                return redirect('/status-bubble/{}'.format(patch_id))
            return HttpResponse("Patch {} already submitted. Please wait for status-bubbles.".format(patch_id))

        rc = BugzillaPatchFetcher().fetch([patch_id])
        if rc == ERR_UNABLE_TO_FETCH_PATCH:
            return HttpResponse('Set r? on patch, EWS is currently unable to access patch {}.'.format(patch_id))
        if rc == ERR_OBSOLETE_PATCH:
            return HttpResponse('Obsolete Patch: {}, not submitting to EWS.'.format(patch_id))
        if rc == ERR_BUG_CLOSED:
            return HttpResponse('Closed Bug for patch: {}, not submitting to EWS.'.format(patch_id))

        if request.POST.get('next_action') == 'return_to_bubbles':
            return redirect('/status-bubble/{}'.format(patch_id))
        return HttpResponse("Submitted patch {} to EWS.".format(patch_id))

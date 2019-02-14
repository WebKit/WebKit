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

from django.http import HttpResponse
from django.shortcuts import render
from django.views import View
from django.views.decorators.clickjacking import xframe_options_exempt
from ews.models.buildermapping import BuilderMapping
from ews.models.patch import Patch
import ews.config as config


class StatusBubble(View):
    def _build_bubble(self, build, patch):
        try:
            builder_display_name = build.builder.display_name
            builder_full_name = build.builder.builder_name
        except BuilderMapping.DoesNotExist:
            builder_display_name = build.builder_id
            builder_full_name = ''

        bubble = {
            "name": builder_display_name,
            "url": 'https://{}/#/builders/{}/builds/{}'.format(config.BUILDBOT_SERVER_HOST, build.builder_id, build.number),
        }

        bubble["details_message"] = '{}\n{}'.format(builder_full_name, build.state_string)
        if build.result is None:
            bubble["state"] = "started"
        elif build.result == 0:  # SUCCESS
            bubble["state"] = "pass"
        elif build.result == 1:  # WARNINGS
            bubble["state"] = "pass"
        elif build.result == 2:  # FAILURE
            bubble["state"] = "fail"
        elif build.result == 3:  # SKIPPED
            bubble["state"] = "none"
        elif build.result == 4:  # EXCEPTION
            bubble["state"] = "error"
        elif build.result == 5:  # RETRY
            bubble["state"] = "provisional-fail"
        else:
            bubble["state"] = "fail"

        return bubble

    def _should_show_bubble_for(self, patch, build):
        # TODO: https://bugs.webkit.org/show_bug.cgi?id=194597
        return True

    def _build_bubbles_for_patch(self, patch):
        show_submit_to_ews = True
        failed_to_apply = False  # TODO: https://bugs.webkit.org/show_bug.cgi?id=194598
        bubbles = []

        if not patch:
            return (None, show_submit_to_ews, failed_to_apply)

        for build in patch.build_set.all():
            show_submit_to_ews = False
            if not self._should_show_bubble_for(patch, build):
                continue
            bubble = self._build_bubble(build, patch)
            if bubble:
                bubbles.append(bubble)

        return (bubbles, show_submit_to_ews, failed_to_apply)

    @xframe_options_exempt
    def get(self, request, patch_id):
        patch_id = int(patch_id)
        patch = Patch.get_patch(patch_id)
        bubbles, show_submit_to_ews, show_failure_to_apply = self._build_bubbles_for_patch(patch)

        template_values = {
            "bubbles": bubbles,
            "patch_id": patch_id,
            "show_submit_to_ews": show_submit_to_ews,
            "show_failure_to_apply": show_failure_to_apply,
        }
        return render(request, 'statusbubble.html', template_values)

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

from django.conf.urls import url

from ews.views.index import Index
from ews.views.patch import Patch
from ews.views.results import Results
from ews.views.statusbubble import StatusBubble
from ews.views.submittoews import SubmitToEWS

app_name = 'ews'
urlpatterns = [
    # ex: /
    url(r'^$', Index.as_view(), name='index'),
    # ex: /patch/5
    url(r'^patch/(?P<patch_id>[0-9]+)/$', Patch.as_view(), name='patch'),
    # ex: /results/
    url(r'^results/$', Results.as_view(), name='results'),
    # ex: /status-bubble/5
    url(r'^status-bubble/(?P<patch_id>[0-9]+)/$', StatusBubble.as_view(), name='statusbubble'),
    # ex: /submit-to-ews/
    url(r'^submit-to-ews/$', SubmitToEWS.as_view(), name='submittoews'),
]

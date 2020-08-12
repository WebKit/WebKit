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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from flask import abort, redirect, request
from resultsdbpy.controller.commit import Commit
from resultsdbpy.flask_support.util import AssertRequest, query_as_kwargs
from resultsdbpy.view.site_menu import SiteMenu


class CIView(object):
    def __init__(self, environment, ci_controller, site_menu=None):
        self.environment = environment
        self.ci_controller = ci_controller
        self.site_menu = site_menu

    @query_as_kwargs()
    def queue(self, **kwargs):
        queue_urls = self.ci_controller.urls_for_queue(**kwargs)

        candidate = None
        for element in queue_urls:
            if not candidate:
                candidate = element['url']
            elif candidate != element['url']:
                abort(404, description='Too many eligible queues')

        if not candidate:
            abort(404, description='No queue URLs for provided arguments')
        return redirect(candidate)

    @query_as_kwargs()
    def worker(self, **kwargs):
        all_urls = self.ci_controller.urls_for_builds(**kwargs)
        worker_urls = []
        for pair in all_urls:
            urls_for_config = []
            for urls in pair['urls']:
                if not urls.get('worker', None):
                    continue
                urls_for_config.append(dict(
                    worker=urls['worker'],
                    uuid=urls['uuid'],
                    start_time=urls['start_time'],
                ))
            if urls_for_config:
                worker_urls.append(dict(configuration=dict(pair['configuration']), urls=urls_for_config))

        if len(worker_urls) == 0:
            abort(404, description='No worker URLs for provided arguments')
        if len(worker_urls) == 1 and len(worker_urls[0]['urls']) == 1:
            return redirect(worker_urls[0]['urls'][0]['worker'])

        abort(404, description='Too many eligible workers')

    @query_as_kwargs()
    def build(self, **kwargs):
        all_urls = self.ci_controller.urls_for_builds(**kwargs)
        worker_urls = []
        for pair in all_urls:
            urls_for_config = []
            for urls in pair['urls']:
                if not urls.get('build', None):
                    continue
                urls_for_config.append(dict(
                    build=urls['build'],
                    uuid=urls['uuid'],
                    start_time=urls['start_time'],
                ))
            if urls_for_config:
                worker_urls.append(dict(configuration=dict(pair['configuration']), urls=urls_for_config))

        candidate = None
        for element in worker_urls:
            for build in element['urls']:
                if not candidate:
                    candidate = build['build']
                elif candidate != build['build']:
                    abort(404, description='Too many eligible builds')

        if not candidate:
            abort(404, description='No queue URLs for provided arguments')
        return redirect(candidate)

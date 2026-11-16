# Copyright (C) 2022 Apple Inc. All rights reserved.
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

from pydoc import describe
from flask import request, abort, jsonify
from resultsdbpy.controller.commit_controller import HasCommitContext


class BugTrackerConfig(object):

    def __init__(self, name):
        self._name = name
        self.commit_context = None

    def name(self):
        return self._name

    def set_commit_context(self, commit_context):
        self.commit_context = commit_context

    def create_bug(self, **kwargs):
        raise NotImplementedError

    @classmethod
    def get_test_result_url(config_params, commit_json, suite, host, protocol='https'):
        build_params = {}
        for k, v in config_params.items():
            build_params[k] = v
        build_params['suite'] = [suite]
        build_params['uuid'] = [commit_json['uuid']]
        build_params['after_time'] = [commit_json['start_time']]
        build_params['before_time'] = [commit_json['start_time']]

        query = []
        for k, v in build_params:
            query.append('{}={}'.format(k, v))

        return '{}://{}/urls/build?{}'.format(protocol, host, '&'.join(query))


class BugTrackerController(HasCommitContext):

    def __init__(self, commit_context, bug_tracker_configs=[]):
        self.bug_tracker_configs = bug_tracker_configs
        for bug_tracker_config in self.bug_tracker_configs:
            bug_tracker_config.set_commit_context(commit_context)
        super(BugTrackerController, self).__init__(commit_context)

    def list_trackers(self):
        bug_tracker_names = []
        if self.bug_tracker_configs:
            for bug_tracker_config in self.bug_tracker_configs:
                bug_tracker_names.append(bug_tracker_config.name())

        return jsonify(bug_tracker_names)

    def create_bug(self, tracker):
        content = request.json
        for bug_tracker_config in self.bug_tracker_configs:
            if bug_tracker_config.name() == tracker:
                try:
                    return jsonify(bug_tracker_config.create_bug(content))
                except ValueError as e:
                    abort(400, description=str(e))

        return abort(404, description='No such bug tracker')

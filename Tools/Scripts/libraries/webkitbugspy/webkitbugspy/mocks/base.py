# Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
import copy

from webkitbugspy import User


class Base(object):
    @classmethod
    def transform_user(cls, user):
        return User(
            name=user.name,
            username=user.username,
            emails=user.emails,
        )

    def __init__(self, users=None, issues=None, projects=None):
        self.users = User.Mapping()
        for user in users or []:
            self.users.add(type(self).transform_user(user))

        self.projects = projects or dict()
        self.issues = {}
        for issue in issues or []:
            self.add(issue)

    def add(self, bug_data):
        if not isinstance(bug_data, dict):
            raise ValueError('Expected bug data to be dictionary')

        id = bug_data.get('id')
        if not id:
            id = 1
            while id in self.issues:
                id += 1
            bug_data['id'] = id

        for user in [bug_data.get('creator'), bug_data.get('assignee')] + bug_data.get('watchers', []):
            if user:
                self.users.add(type(self).transform_user(user))

        self.issues[id] = copy.deepcopy(bug_data)
        return id

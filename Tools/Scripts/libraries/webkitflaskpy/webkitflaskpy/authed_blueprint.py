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

import re

from flask import abort, request as frequest, Blueprint


class AuthedBlueprint(Blueprint):
    @classmethod
    def blocked_user_agent_decorator(cls, method, user_agents):

        def real_method(*args, **kwargs):
            user_agent = frequest.headers.get('User-Agent') or ''
            for regex in user_agents:
                if regex.match(user_agent):
                    abort(404, description='No data available for user')

            return method(*args, **kwargs)

        real_method.__name__ = method.__name__
        return real_method

    def __init__(self, name, import_name, auth_decorator=None, blocked_user_agents=None, **kwargs):
        super(AuthedBlueprint, self).__init__(name, import_name, **kwargs)
        self.auth_decorator = auth_decorator
        self.blocked_user_agents = [
            re.compile(value) if isinstance(value, str) else value
            for value in blocked_user_agents or []
        ]

    def add_url_rule(self, rule, endpoint=None, view_func=None, authed=True, **options):
        if self.blocked_user_agents:
            view_func = self.blocked_user_agent_decorator(view_func, self.blocked_user_agents)
        if not self.auth_decorator or not authed:
            return super(AuthedBlueprint, self).add_url_rule(rule, endpoint=endpoint, view_func=view_func, **options)
        return super(AuthedBlueprint, self).add_url_rule(rule, endpoint=endpoint, view_func=self.auth_decorator(view_func), **options)

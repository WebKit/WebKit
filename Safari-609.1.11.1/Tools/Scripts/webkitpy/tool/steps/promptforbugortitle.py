# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from webkitpy.tool.steps.abstractstep import AbstractStep
from webkitpy.tool.steps.options import Options
from urlparse import urlparse
import re

class PromptForBugOrTitle(AbstractStep):
    @classmethod
    def options(cls):
        return AbstractStep.options() + [
            Options.non_interactive,
        ]

    def run(self, state):
        # No need to prompt if we alrady have the bug_id.
        if state.get("bug_id"):
            return
        user_response = self._tool.user.prompt("Please enter a bug number/bugzilla URL or a title for a new bug:\n")
        # If the user responds with a number or a valid bugzilla URL, we assume it's bug number.
        # Otherwise we assume it's a bug subject.
        try:
            state["bug_id"] = int(user_response)
        except ValueError as TypeError:
            parsed_url = None
            try:
                parsed_url = urlparse(user_response)
            except ValueError:
                # urlparse can throw a value error for some strings.
                pass

            if parsed_url and re.match("bugs.webkit.org", parsed_url.netloc):
                    match = re.match("id=(?P<bug_id>\d+)", parsed_url.query)
                    if match:
                        state["bug_id"] = int(match.group("bug_id"))
                        return

            if not self._options.non_interactive and not self._tool.user.confirm("Are you sure you want to create a new bug?", default="n"):
                self._exit(1)
            state["bug_title"] = user_response
            # FIXME: This is kind of a lame description.
            state["bug_description"] = user_response

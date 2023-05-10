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

import re

from webkitcorepy import string_utils

from webkitpy.common.checkout.changelog import ChangeLog
from webkitpy.common.config import urls
from webkitpy.tool.steps.abstractstep import AbstractStep


class PrepareChangeLogForRevert(AbstractStep):
    INTEGER_RE = re.compile(r'^\d+$')

    @classmethod
    def _message_for_revert(cls, revision_list, reason, description_list, reverted_bug_url_list, revert_bug_url=None):
        message = "Unreviewed, reverting {}.\n".format(string_utils.join([
            '{}{}'.format('r' if cls.INTEGER_RE.match(str(revision)) else '', revision) for revision in revision_list
        ]))
        if revert_bug_url:
            message += "%s\n" % revert_bug_url
        message += "\n"
        if reason:
            message += "%s\n" % reason
        message += "\n"
        message += "Reverted changeset%s:\n\n" % ('s' if len(revision_list) > 1 else '')
        for index in range(len(revision_list)):
            if description_list[index]:
                message += "\"%s\"\n" % description_list[index]
            if reverted_bug_url_list[index]:
                message += "%s\n" % reverted_bug_url_list[index]
            message += "%s\n\n" % urls.view_revision_url(revision_list[index])
        return message

    def run(self, state):
        reverted_bug_url_list = []
        revert_bug_url = self._tool.bugs.bug_url_for_bug_id(state["bug_id"]) if state["bug_id"] else None
        for bug_id in state["bug_id_list"]:
            reverted_bug_url_list.append(self._tool.bugs.bug_url_for_bug_id(bug_id))
        message = self._message_for_revert(state["revision_list"], state["reason"], state["description_list"], reverted_bug_url_list, revert_bug_url)
        self._tool.executive.run_and_throw_if_fail(['git', 'commit', '-a', '-m', message], cwd=self._tool.scm().checkout_root)

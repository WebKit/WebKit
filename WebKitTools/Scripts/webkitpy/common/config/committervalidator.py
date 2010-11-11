# Copyright (c) 2009 Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
# Copyright (c) 2010 Research In Motion Limited. All rights reserved.
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

import os

from webkitpy.common.system.ospath import relpath
from webkitpy.common.config import committers


class CommitterValidator(object):

    def __init__(self, bugzilla):
        self._bugzilla = bugzilla

    # _view_source_url belongs in some sort of webkit_config.py module.
    def _view_source_url(self, local_path):
        return "http://trac.webkit.org/browser/trunk/%s" % local_path

    def _checkout_root(self):
        # FIXME: This is a hack, we would have this from scm.checkout_root
        # if we had any way to get to an scm object here.
        components = __file__.split(os.sep)
        tools_index = components.index("WebKitTools")
        return os.sep.join(components[:tools_index])

    def _committers_py_path(self):
        # extension can sometimes be .pyc, we always want .py
        (path, extension) = os.path.splitext(committers.__file__)
        # FIXME: When we're allowed to use python 2.6 we can use the real
        # os.path.relpath
        path = relpath(path, self._checkout_root())
        return ".".join([path, "py"])

    def _flag_permission_rejection_message(self, setter_email, flag_name):
        # Should come from some webkit_config.py
        contribution_guidlines = "http://webkit.org/coding/contributing.html"
        # This could be queried from the status_server.
        queue_administrator = "eseidel@chromium.org"
        # This could be queried from the tool.
        queue_name = "commit-queue"
        committers_list = self._committers_py_path()
        message = "%s does not have %s permissions according to %s." % (
                        setter_email,
                        flag_name,
                        self._view_source_url(committers_list))
        message += "\n\n- If you do not have %s rights please read %s for instructions on how to use bugzilla flags." % (
                        flag_name, contribution_guidlines)
        message += "\n\n- If you have %s rights please correct the error in %s by adding yourself to the file (no review needed).  " % (
                        flag_name, committers_list)
        message += "The %s restarts itself every 2 hours.  After restart the %s will correctly respect your %s rights." % (
                        queue_name, queue_name, flag_name)
        return message

    def _validate_setter_email(self, patch, result_key, rejection_function):
        committer = getattr(patch, result_key)()
        # If the flag is set, and we don't recognize the setter, reject the
        # flag!
        setter_email = patch._attachment_dictionary.get("%s_email" % result_key)
        if setter_email and not committer:
            rejection_function(patch.id(),
                self._flag_permission_rejection_message(setter_email,
                                                        result_key))
            return False
        return True

    def _reject_patch_if_flags_are_invalid(self, patch):
        return (self._validate_setter_email(
                patch, "reviewer", self.reject_patch_from_review_queue)
            and self._validate_setter_email(
                patch, "committer", self.reject_patch_from_commit_queue))

    def patches_after_rejecting_invalid_commiters_and_reviewers(self, patches):
        return [patch for patch in patches if self._reject_patch_if_flags_are_invalid(patch)]

    def reject_patch_from_commit_queue(self,
                                       attachment_id,
                                       additional_comment_text=None):
        comment_text = "Rejecting patch %s from commit-queue." % attachment_id
        self._bugzilla.set_flag_on_attachment(attachment_id,
                                              "commit-queue",
                                              "-",
                                              comment_text,
                                              additional_comment_text)

    def reject_patch_from_review_queue(self,
                                       attachment_id,
                                       additional_comment_text=None):
        comment_text = "Rejecting patch %s from review queue." % attachment_id
        self._bugzilla.set_flag_on_attachment(attachment_id,
                                              'review',
                                              '-',
                                              comment_text,
                                              additional_comment_text)

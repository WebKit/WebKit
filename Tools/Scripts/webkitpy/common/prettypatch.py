# Copyright (c) 2010 Google Inc. All rights reserved.
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

import errno
import logging
import os
import tempfile
from webkitpy.common.system.executive import ScriptError

_log = logging.getLogger(__name__)


class PrettyPatch(object):
    # FIXME: PrettyPatch should not require checkout_root.
    def __init__(self, executive, checkout_root, filesystem=None):
        self._executive = executive
        self._filesystem = filesystem
        self._checkout_root = checkout_root
        self._pretty_patch_path = os.path.join(self._checkout_root, "Websites", "bugs.webkit.org", "PrettyPatch")
        self._prettify_path = os.path.join(self._pretty_patch_path, "prettify.rb")
        self.pretty_patch_error_html = "Failed to run PrettyPatch, see error log."
        self.ppatch_available = None

    def pretty_diff_file(self, diff):
        # Diffs can contain multiple text files of different encodings
        # so we always deal with them as byte arrays, not unicode strings.
        assert(isinstance(diff, str))
        pretty_diff = self.pretty_diff(diff)
        diff_file = tempfile.NamedTemporaryFile(suffix=".html")
        diff_file.write(pretty_diff)
        diff_file.flush()
        return diff_file

    def pretty_diff(self, diff):
        # pretify.rb will hang forever if given no input.
        # Avoid the hang by returning an empty string.
        if not diff:
            return ""

        args = [
            "ruby",
            "-I",
            self._pretty_patch_path,
            self._prettify_path,
        ]
        # PrettyPatch does not modify the encoding of the diff output
        # so we can't expect it to be utf-8.
        return self._executive.run_command(args, input=diff, decode_output=False)

    def pretty_patch_available(self):
        if self.ppatch_available is None:
            self.ppatch_available = self.check_pretty_patch(logging=False)
        return self.ppatch_available

    def check_pretty_patch(self, logging=True):
        """Checks whether we can use the PrettyPatch ruby script."""
        try:
            _ = self._executive.run_command(['ruby', '--version'])
        except OSError as e:
            if e.errno in [errno.ENOENT, errno.EACCES, errno.ECHILD]:
                if logging:
                    _log.warning("Ruby is not installed; can't generate pretty patches.")
                    _log.warning('')
                return False

        if not self._filesystem.exists(self._pretty_patch_path):
            if logging:
                _log.warning("Unable to find %s; can't generate pretty patches." % self._pretty_patch_path)
                _log.warning('')
            return False

        return True

    def pretty_patch_text(self, diff_path):
        if self.ppatch_available is None:
            self.ppatch_available = self.check_pretty_patch(logging=False)
        if not self.ppatch_available:
            return self.pretty_patch_error_html
        command = ("ruby", "-I", self._filesystem.dirname(self._prettify_path),
                   self._prettify_path, diff_path)
        try:
            # Diffs are treated as binary (we pass decode_output=False) as they
            # may contain multiple files of conflicting encodings.
            return self._executive.run_command(command, decode_output=False)
        except OSError as e:
            # If the system is missing ruby log the error and stop trying.
            self.ppatch_available = False
            _log.error("Failed to run PrettyPatch (%s): %s" % (command, e))
            return self.pretty_patch_error_html
        except ScriptError as e:
            # If ruby failed to run for some reason, log the command
            # output and stop trying.
            self.ppatch_available = False
            _log.error("Failed to run PrettyPatch (%s):\n%s" % (command, e.message_with_output()))
            return self.pretty_patch_error_html

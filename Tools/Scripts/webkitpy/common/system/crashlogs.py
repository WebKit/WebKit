# Copyright (c) 2011, Google Inc. All rights reserved.
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
import sys


class CrashLogs(object):
    def __init__(self, filesystem):
        self._filesystem = filesystem

    def find_newest_log(self, process_name, pid=None):
        if sys.platform == "darwin":
            return self._find_newest_log_darwin(process_name, pid)

    def _log_directory_darwin(self):
        log_directory = self._filesystem.expanduser("~")
        log_directory = self._filesystem.join(log_directory, "Library", "Logs")
        if self._filesystem.exists(self._filesystem.join(log_directory, "DiagnosticReports")):
            log_directory = self._filesystem.join(log_directory, "DiagnosticReports")
        else:
            log_directory = self._filesystem.join(log_directory, "CrashReporter")
        return log_directory

    def _find_newest_log_darwin(self, process_name, pid):
        def is_crash_log(fs, dirpath, basename):
            return basename.startswith(process_name + "_") and basename.endswith(".crash")

        log_directory = self._log_directory_darwin()
        logs = self._filesystem.files_under(log_directory, file_filter=is_crash_log)
        if not logs:
            return None
        for path in reversed(sorted(logs)):
            if pid is not None:
                try:
                    app_description = self._filesystem.getxattr(path, 'app_description')
                    if not app_description.startswith('%s[%d] version ' % (process_name, pid)):
                        continue
                except KeyError:
                    # The file doesn't have the app_description extended attribute for some reason.
                    continue
            return self._filesystem.read_text_file(path)

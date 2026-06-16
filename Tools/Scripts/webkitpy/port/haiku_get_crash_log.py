# Copyright (C) 2013 Haiku, inc.
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

import subprocess
import os


class HaikuCrashLogGenerator(object):
    def __init__(self, name, pid, newer_than, filesystem, path_to_driver):
        self.name = name
        self.pid = pid
        self.newer_than = newer_than
        self._filesystem = filesystem
        self._path_to_driver = path_to_driver

    def _get_debugger_output(self, coredump_path):
        result = None
        try:
            with open(coredump_path, 'rb') as corefile:
                result = corefile.read().decode('utf8', 'ignore')
            os.unlink(coredump_path)
        except IOError:
            result = None
        return result

    def generate_crash_log(self, stdout, stderr):
        pid_representation = str(self.pid or '<unknown>')
        log_directory = os.environ.get("WEBKIT_CORE_DUMPS_DIRECTORY")
        errors = []
        crash_log = ''
        expected_crash_dump_filename = "DumpRenderTree-%s-debug" % (pid_representation)
        proc_name = "%s" % (self.name)

        def match_filename(filesystem, directory, filename):
            return filename.find(expected_crash_dump_filename) > -1

        if not log_directory:
            log_directory = "/boot/home/Desktop"

        dumps = self._filesystem.files_under(log_directory, file_filter=match_filename)
        if dumps:
            # Get the most recent coredump matching the pid and/or process name.
            coredump_path = list(reversed(sorted(dumps)))[0]
            if not self.newer_than or self._filesystem.mtime(coredump_path) > self.newer_than:
                crash_log = self._get_debugger_output(coredump_path)
        core_pattern = os.path.join(log_directory, expected_crash_dump_filename)

        if not crash_log:
            crash_log = """\
Crash report %(expected_crash_dump_filename)s not found. To enable crash logs,
create the file ~/config/settings/system/debug_server/settings with the following contents:

executable_actions {
    DumpRenderTree log
}
""" % locals()

        return (stderr, """\
crash log for %(proc_name)s (pid %(pid_representation)s):

%(crash_log)s""" % locals())

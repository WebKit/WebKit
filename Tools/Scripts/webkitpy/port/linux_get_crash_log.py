# Copyright (C) 2013 University of Szeged
# This module is a refactoring from gtk.py, Copyright (C) 2013 Igalia S.L
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

import subprocess
import os


class GDBCrashLogGenerator(object):
    def __init__(self, name, pid, newer_than, filesystem, path_to_driver):
        self.name = name
        self.pid = pid
        self.newer_than = newer_than
        self._filesystem = filesystem
        self._path_to_driver = path_to_driver

    def _get_gdb_output(self, coredump_path):
        cmd = ['gdb', '-ex', 'thread apply all bt 1024', '--batch', str(self._path_to_driver()), coredump_path]
        proc = subprocess.Popen(cmd, stdin=None, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        errors = [stderr_line.strip().decode('utf8', 'ignore') for stderr_line in stderr.splitlines()]
        return (stdout.decode('utf8', 'ignore'), errors)

    def generate_crash_log(self, stdout, stderr):
        pid_representation = str(self.pid or '<unknown>')
        log_directory = os.environ.get("WEBKIT_CORE_DUMPS_DIRECTORY")
        errors = []
        crash_log = ''
        expected_crash_dump_filename = "core-pid_%s-_-process_%s" % (pid_representation, self.name)
        proc_name = "%s" % (self.name)

        def match_filename(filesystem, directory, filename):
            if self.pid:
                return filename == expected_crash_dump_filename
            return filename.find(self.name) > -1

        if log_directory:
            dumps = self._filesystem.files_under(log_directory, file_filter=match_filename)
            if dumps:
                # Get the most recent coredump matching the pid and/or process name.
                coredump_path = list(reversed(sorted(dumps)))[0]
                if not self.newer_than or self._filesystem.mtime(coredump_path) > self.newer_than:
                    crash_log, errors = self._get_gdb_output(coredump_path)

        stderr_lines = errors + str(stderr or '<empty>').decode('utf8', 'ignore').splitlines()
        errors_str = '\n'.join(('STDERR: ' + stderr_line) for stderr_line in stderr_lines)
        if not crash_log:
            if not log_directory:
                log_directory = "/path/to/coredumps"
            core_pattern = os.path.join(log_directory, "core-pid_%p-_-process_%e")
            crash_log = """\
Coredump %(expected_crash_dump_filename)s not found. To enable crash logs:

- run this command as super-user: echo "%(core_pattern)s" > /proc/sys/kernel/core_pattern
- enable core dumps: ulimit -c unlimited
- set the WEBKIT_CORE_DUMPS_DIRECTORY environment variable: export WEBKIT_CORE_DUMPS_DIRECTORY=%(log_directory)s

""" % locals()

        return (stderr, """\
crash log for %(proc_name)s (pid %(pid_representation)s):

%(crash_log)s
%(errors_str)s""" % locals())

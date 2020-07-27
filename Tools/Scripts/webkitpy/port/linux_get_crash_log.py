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

import os
import re
import subprocess
import tempfile
import time

from webkitcorepy import string_utils

from webkitpy.common.system.executive import ScriptError
from webkitpy.common.webkit_finder import WebKitFinder


class GDBCrashLogGenerator(object):
    _find_pid_regex = re.compile(r'PID: (\d+) \(.*\)')

    def __init__(self, executive, name, pid, newer_than, filesystem, path_to_driver, port_name, configuration):
        self.name = name
        self.pid = pid
        self.newer_than = newer_than
        self._filesystem = filesystem
        self._path_to_driver = path_to_driver
        self._executive = executive
        self._port_name = port_name
        self._configuration = configuration
        self._webkit_finder = WebKitFinder(filesystem)

    def _get_gdb_output(self, coredump_path):
        process_name = self._filesystem.join(os.path.dirname(str(self._path_to_driver())), self.name)
        cmd = ['gdb', '-ex', 'thread apply all bt 1024', '--batch', process_name, coredump_path]
        proc = self._executive.popen(cmd, stdin=None, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        errors = [stderr_line.strip().decode('utf8', 'ignore') for stderr_line in stderr.splitlines()]
        if proc.returncode != 0:
            stdout = ('ERROR: The gdb process exited with non-zero return code %s\n\n' % proc.returncode) + stdout
        return (stdout.decode('utf8', 'ignore'), errors)

    def _get_tmp_file_name(self, coredumpctl, filename):
        if coredumpctl[0] == 'flatpak-spawn':
            return "/run/host/" + filename

        return filename

    def _get_trace_from_systemd(self, coredumpctl, pid):
        if os.path.isfile("/.flatpak-info"):
            return self._get_trace_from_flatpak()

        # Letting up to 5 seconds for the backtrace to be generated on the systemd side
        for try_number in range(5):
            if try_number != 0:
                # Looping, it means we consider the logs might not be ready yet.
                time.sleep(1)

            try:
                info = self._executive.run_command(coredumpctl + ['info', "--since=" + time.strftime("%a %Y-%m-%d %H:%M:%S %Z", time.localtime(self.newer_than))],
                    return_stderr=True)
            except (ScriptError, OSError):
                continue

            found_newer = False
            # Coredumpctl will use the latest core dump with the specified PID
            # assume it is the right one.
            pids = self._find_pid_regex.findall(info)
            if not pids:
                continue

            pid = pids[0]
            with tempfile.NamedTemporaryFile() as temp_file:
                if self._executive.run_command(coredumpctl + ['dump', pid, '--output',
                        temp_file.name], return_exit_code=True):
                    continue

                return self._get_gdb_output(self._get_tmp_file_name(coredumpctl, temp_file.name))

        return '', []

    def _get_trace_from_flatpak(self):
        if self.newer_than:
            coredump_since = "--gdb-stack-trace=@%f" % self.newer_than
        else:
            coredump_since = "--gdb-stack-trace"
        webkit_flatpak_path = self._webkit_finder.path_to_script('webkit-flatpak')
        cmd = ['flatpak-spawn', '--host', webkit_flatpak_path, '--%s' % self._port_name,
               "--%s" % self._configuration.lower(), coredump_since]

        proc = self._executive.popen(cmd, stdin=None, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        crash_log, stderr = proc.communicate()
        errors = string_utils.decode(str(stderr or '<empty>'), errors='ignore').splitlines()
        return crash_log, errors

    def generate_crash_log(self, stdout, stderr):
        pid_representation = str(self.pid or '<unknown>')
        log_directory = os.environ.get("WEBKIT_CORE_DUMPS_DIRECTORY")
        errors = []
        crash_log = ''
        expected_crash_dump_filename = "core-pid_%s.dump" % pid_representation
        proc_name = "%s" % (self.name)

        def match_filename(filesystem, directory, filename):
            if self.pid:
                return filename == expected_crash_dump_filename
            return filename.find(self.name) > -1

        # Poor man which, ignore any failure.
        for coredumpctl in [['coredumpctl'], ['flatpak-spawn', '--host', 'coredumpctl'], []]:
            try:
                if not self._executive.run_command(coredumpctl, return_exit_code=True):
                    break
            except:
                continue

        if log_directory:
            dumps = self._filesystem.files_under(
                log_directory, file_filter=match_filename)
            if dumps:
                # Get the most recent coredump matching the pid and/or process name.
                coredump_path = list(reversed(sorted(dumps)))[0]
                if not self.newer_than or self._filesystem.mtime(coredump_path) > self.newer_than:
                    crash_log, errors = self._get_gdb_output(coredump_path)
        elif coredumpctl:
            crash_log, errors = self._get_trace_from_systemd(coredumpctl, pid_representation)

        stderr_lines = errors + string_utils.decode(str(stderr or '<empty>'), errors='ignore').splitlines()
        errors_str = '\n'.join(('STDERR: ' + stderr_line) for stderr_line in stderr_lines)
        cppfilt_proc = self._executive.popen(
            ['c++filt'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        errors_str = cppfilt_proc.communicate(errors_str)[0]

        if not crash_log:
            if not log_directory:
                log_directory = "/path/to/coredumps"
            core_pattern = self._filesystem.join(log_directory, "core-pid_%p.dump")
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

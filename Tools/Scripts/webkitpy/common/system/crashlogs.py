# Copyright (c) 2011, Google Inc. All rights reserved.
# Copyright (c) 2015, Apple Inc. All rights reserved.
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

import codecs
import re
import datetime


class CrashLogs(object):

    GLOBAL_PID_REGEX = re.compile(r'\s+Global\s+PID:\s+\[(?P<pid>\d+)\]')
    EXIT_PROCESS_PID_REGEX = re.compile(r'Exit process \d+:(?P<pid>\w+), code')

    def __init__(self, host, results_directory=None):
        self._host = host
        self._results_directory = results_directory

    def find_newest_log(self, process_name, pid=None, include_errors=False, newer_than=None):
        if self._host.platform.is_mac():
            return self._find_newest_log_darwin(process_name, pid, include_errors, newer_than)
        elif self._host.platform.is_win():
            return self._find_newest_log_win(process_name, pid, include_errors, newer_than)
        return None

    def find_all_logs(self, include_errors=False, newer_than=None):
        if self._host.platform.is_mac():
            return self._find_all_logs_darwin(include_errors, newer_than)
        return None

    def _log_directory_darwin(self):
        log_directory = self._host.filesystem.expanduser("~")
        log_directory = self._host.filesystem.join(log_directory, "Library", "Logs")
        if self._host.filesystem.exists(self._host.filesystem.join(log_directory, "DiagnosticReports")):
            log_directory = self._host.filesystem.join(log_directory, "DiagnosticReports")
        else:
            log_directory = self._host.filesystem.join(log_directory, "CrashReporter")
        return log_directory

    def _find_newest_log_darwin(self, process_name, pid, include_errors, newer_than):
        def is_crash_log(fs, dirpath, basename):
            return basename.startswith(process_name + "_") and basename.endswith(".crash")

        log_directory = self._log_directory_darwin()
        logs = self._host.filesystem.files_under(log_directory, file_filter=is_crash_log)
        first_line_regex = re.compile(r'^Process:\s+(?P<process_name>.*) \[(?P<pid>\d+)\]$')
        errors = ''
        for path in reversed(sorted(logs)):
            try:
                if not newer_than or self._host.filesystem.mtime(path) > newer_than:
                    f = self._host.filesystem.read_text_file(path)
                    match = first_line_regex.match(f[0:f.find('\n')])
                    if match and match.group('process_name') == process_name and (pid is None or int(match.group('pid')) == pid):
                        return errors + f
            except IOError, e:
                if include_errors:
                    errors += "ERROR: Failed to read '%s': %s\n" % (path, str(e))
            except OSError, e:
                if include_errors:
                    errors += "ERROR: Failed to read '%s': %s\n" % (path, str(e))

        if include_errors and errors:
            return errors
        return None

    def _find_newest_log_win(self, process_name, pid, include_errors, newer_than):
        def is_crash_log(fs, dirpath, basename):
            return basename.startswith("CrashLog")

        logs = self._host.filesystem.files_under(self._results_directory, file_filter=is_crash_log)
        errors = u''
        for path in reversed(sorted(logs)):
            try:
                if not newer_than or self._host.filesystem.mtime(path) > newer_than:
                    log_file = self._host.filesystem.read_binary_file(path).decode('ascii', 'ignore')
                    match = self.GLOBAL_PID_REGEX.search(log_file)
                    if match:
                        if int(match.group('pid')) == pid:
                            return errors + log_file
                    match = self.EXIT_PROCESS_PID_REGEX.search(log_file)
                    if match is None:
                        continue
                    # Note: This output comes from a program that shows PID in hex:
                    if int(match.group('pid'), 16) == pid:
                        return errors + log_file
            except IOError, e:
                print "IOError %s" % str(e)
                if include_errors:
                    errors += u"ERROR: Failed to read '%s': %s\n" % (path, str(e))
            except OSError, e:
                print "OSError %s" % str(e)
                if include_errors:
                    errors += u"ERROR: Failed to read '%s': %s\n" % (path, str(e))
            except UnicodeDecodeError, e:
                print "UnicodeDecodeError %s" % str(e)
                if include_errors:
                    errors += u"ERROR: Failed to decode '%s' as ascii: %s\n" % (path, str(e))

        if include_errors and errors:
            return errors
        return None

    def _find_all_logs_darwin(self, include_errors, newer_than):
        def is_crash_log(fs, dirpath, basename):
            return basename.endswith(".crash")

        log_directory = self._log_directory_darwin()
        logs = self._host.filesystem.files_under(log_directory, file_filter=is_crash_log)
        first_line_regex = re.compile(r'^Process:\s+(?P<process_name>.*) \[(?P<pid>\d+)\]$')
        errors = ''
        crash_logs = {}
        for path in reversed(sorted(logs)):
            try:
                if not newer_than or self._host.filesystem.mtime(path) > newer_than:
                    result_name = "Unknown"
                    pid = 0
                    log_contents = self._host.filesystem.read_text_file(path)
                    # Verify timestamp from log contents
                    crash_time = self.get_timestamp_from_log(log_contents)
                    if crash_time is not None and newer_than is not None:
                        start_time = datetime.datetime.fromtimestamp(float(newer_than))
                        if crash_time < start_time:
                            continue

                    match = first_line_regex.match(log_contents[0:log_contents.find('\n')])
                    if match:
                        process_name = match.group('process_name')
                        pid = str(match.group('pid'))
                        result_name = process_name + "-" + pid

                    while result_name in crash_logs:
                        result_name = result_name + "-1"
                    crash_logs[result_name] = errors + log_contents
            except IOError, e:
                if include_errors:
                    errors += "ERROR: Failed to read '%s': %s\n" % (path, str(e))
            except OSError, e:
                if include_errors:
                    errors += "ERROR: Failed to read '%s': %s\n" % (path, str(e))

        if include_errors and errors and len(crash_logs) == 0:
            return errors
        return crash_logs

    def get_timestamp_from_log(self, log_contents):
        date_match = re.search('Date/Time:\s+(.+?)\n', log_contents)
        if not date_match:
            return None
        try:
            crash_time_str = ' '.join(date_match.group(1).split(" ")[0:2])
            crash_time = datetime.datetime.strptime(crash_time_str, '%Y-%m-%d %H:%M:%S.%f')
        except ValueError:
            return None
        return crash_time

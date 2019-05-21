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

import datetime
import logging
import re


_log = logging.getLogger(__name__)


class CrashLogs(object):

    # Matches a string like '    Global    D1    PID: [14516]'
    GLOBAL_PID_REGEX = re.compile(r'\s+Global\b.+\bPID:\s+\[(?P<pid>\d+)\]')
    EXIT_PROCESS_PID_REGEX = re.compile(r'Exit process \d+:(?P<pid>\w+), code')
    DARWIN_PROCESS_REGEX = re.compile(r'^Process:\s+(?P<process_name>.*) \[(?P<pid>\d+)\]$')

    def __init__(self, host, crash_log_directory, crash_logs_to_skip=[]):
        self._host = host
        self._crash_log_directory = crash_log_directory
        self._crash_logs_to_skip = crash_logs_to_skip

    def find_newest_log(self, process_name, pid=None, include_errors=False, newer_than=None):
        if self._host.platform.is_mac() or self._host.platform.is_ios():
            return self._find_newest_log_darwin(process_name, pid, include_errors, newer_than)
        elif self._host.platform.is_win():
            return self._find_newest_log_win(process_name, pid, include_errors, newer_than)
        return None

    def find_all_logs(self, include_errors=False, newer_than=None):
        if self._host.platform.is_mac() or self._host.platform.is_ios():
            return self._find_all_logs_darwin(include_errors, newer_than)
        return None

    def _parse_darwin_crash_log(self, path):
        contents = self._host.symbolicate_crash_log_if_needed(path)
        if not contents:
            return (None, None, None)
        is_sandbox_violation = False
        for line in contents.splitlines():
            if line.startswith('Sandbox Violation:'):
                is_sandbox_violation = True
            match = CrashLogs.DARWIN_PROCESS_REGEX.match(line)
            if match:
                return (('Sandbox-' if is_sandbox_violation else '') + match.group('process_name'), int(match.group('pid')), contents)
        return (None, None, contents)

    def _find_newest_log_darwin(self, process_name, pid, include_errors, newer_than):
        def is_crash_log(fs, dirpath, basename):
            if self._crash_logs_to_skip and fs.join(dirpath, basename) in self._crash_logs_to_skip:
                return False
            return (basename.startswith(process_name + '_') and (basename.endswith('.crash')) or
                    (process_name in basename  and basename.endswith('.ips')))

        logs = self._host.filesystem.files_under(self._crash_log_directory, file_filter=is_crash_log)
        errors = ''
        for path in reversed(sorted(logs)):
            try:
                if not newer_than or self._host.filesystem.mtime(path) > newer_than:
                    parsed_name, parsed_pid, log_contents = self._parse_darwin_crash_log(path)
                    if parsed_name == process_name and (pid is None or parsed_pid == pid):
                        return errors + log_contents
            except IOError as e:
                if include_errors:
                    errors += "ERROR: Failed to read '%s': %s\n" % (path, str(e))
            except OSError as e:
                if include_errors:
                    errors += "ERROR: Failed to read '%s': %s\n" % (path, str(e))

        if include_errors and errors:
            return errors
        return None

    def _find_newest_log_win(self, process_name, pid, include_errors, newer_than):
        def is_crash_log(fs, dirpath, basename):
            if self._crash_logs_to_skip and fs.join(dirpath, basename) in self._crash_logs_to_skip:
                return False
            return basename.startswith("CrashLog")

        logs = self._host.filesystem.files_under(self._crash_log_directory, file_filter=is_crash_log)
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
            except IOError as e:
                if include_errors:
                    errors += u"ERROR: Failed to read '%s': %s\n" % (path, str(e))
            except OSError as e:
                if include_errors:
                    errors += u"ERROR: Failed to read '%s': %s\n" % (path, str(e))
            except UnicodeDecodeError as e:
                if include_errors:
                    errors += u"ERROR: Failed to decode '%s' as ascii: %s\n" % (path, str(e))

        if include_errors and errors:
            return errors
        return None

    def _find_all_logs_darwin(self, include_errors, newer_than):
        def is_crash_log(fs, dirpath, basename):
            if self._crash_logs_to_skip and fs.join(dirpath, basename) in self._crash_logs_to_skip:
                return False
            return basename.endswith('.crash') or basename.endswith('.ips')

        logs = self._host.filesystem.files_under(self._crash_log_directory, file_filter=is_crash_log)
        errors = ''
        crash_logs = {}
        for path in reversed(sorted(logs)):
            try:
                if not newer_than or self._host.filesystem.mtime(path) > newer_than:
                    result_name = "Unknown"
                    parsed_name, parsed_pid, log_contents = self._parse_darwin_crash_log(path)
                    if not log_contents:
                        _log.warn('No data in crash log at {}'.format(path))
                        continue

                    # Verify timestamp from log contents
                    crash_time = self.get_timestamp_from_log(log_contents)
                    if crash_time is not None and newer_than is not None:
                        start_time = datetime.datetime.fromtimestamp(float(newer_than))
                        if crash_time < start_time:
                            continue

                    if parsed_name:
                        result_name = parsed_name + "-" + str(parsed_pid)

                    # Processes can remain running after Sandbox violations, which generate crash logs.
                    # This means that we can have mutliple crash logs attributed to the same process.
                    # The unique_name must be named in the format PROCESS_NAME-PID-# or Sandbox-PROCESS_NAME-PID-#,
                    # where '-#' is optional. This is because of how DarwinPort._merge_crash_logs parses the crash name.
                    count = 1
                    unique_name = result_name
                    while unique_name in crash_logs:
                        unique_name = result_name + '-' + str(count)
                        count += 1
                    crash_logs[unique_name] = errors + log_contents
            except IOError as e:
                if include_errors:
                    errors += "ERROR: Failed to read '%s': %s\n" % (path, str(e))
            except OSError as e:
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

# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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
#     * Neither the Google name nor the names of its
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

import logging
import re

from webkitpy.common.system.executive import ScriptError

_log = logging.getLogger(__name__)


# If other ports/platforms decide to support --leaks, we should see about sharing as much of this code as possible.
# Right now this code is only used by Apple's MacPort.

class LeakDetector(object):
    def __init__(self, port):
        # We should operate on a "platform" not a port here.
        self._port = port
        self._executive = port._executive
        self._filesystem = port._filesystem

    # We exclude the following reported leaks so they do not get in our way when looking for WebKit leaks:
    # This allows us ignore known leaks and only be alerted when new leaks occur. Some leaks are in the old
    # versions of the system frameworks that are being used by the leaks bots. Even though a leak has been
    # fixed, it will be listed here until the bot has been updated with the newer frameworks.
    def _types_to_exclude_from_leaks(self):
        # Currently we don't have any type excludes from OS leaks, but we will likely again in the future.
        return []

    def _callstacks_to_exclude_from_leaks(self):
        callstacks = [
            'WTF::BitVector::OutOfLineBits::create', # https://bugs.webkit.org/show_bug.cgi?id=121662
            'WTF::BitVector::resizeOutOfLine', # https://bugs.webkit.org/show_bug.cgi?id=121662
            'WebCore::createPrivateStorageSession', # <rdar://problem/35189565>
            'CIDeviceManagerStartMonitoring', # <rdar://problem/35711052>
            'NSSpellChecker init', # <rdar://problem/35434615>
            'NSColor controlHighlightColor', # <rdar://problem/35816332>
        ]
        return callstacks

    def _leaks_args(self, process_name, process_pid):
        leaks_args = []
        for callstack in self._callstacks_to_exclude_from_leaks():
            leaks_args += ['--exclude-callstack=%s' % callstack]
        for excluded_type in self._types_to_exclude_from_leaks():
            leaks_args += ['--exclude-type=%s' % excluded_type]
        leaks_args += ['--output-file=%s' % self._filesystem.join(self._port.results_directory(), self.leaks_file_name(process_name, process_pid))]
        leaks_args += ['--memgraph-file=%s' % self._filesystem.join(self._port.results_directory(), self.memgraph_file_name(process_name, process_pid))]
        leaks_args.append(process_pid)
        return leaks_args

    def _parse_leaks_output(self, leaks_output):
        if not leaks_output:
            return 0, 0, 0
        _, count, bytes = re.search(r'Process (?P<pid>\d+): (?P<count>\d+) leaks? for (?P<bytes>\d+) total', leaks_output).groups()
        excluded_match = re.search(r'(?P<excluded>\d+) leaks? excluded', leaks_output)
        excluded = excluded_match.group('excluded') if excluded_match else 0
        return int(count), int(excluded), int(bytes)

    def leaks_files_in_directory(self, directory):
        return self._filesystem.glob(self._filesystem.join(directory, "*-leaks.txt"))

    def leaks_file_name(self, process_name, process_pid):
        return "%s-%s-leaks.txt" % (process_name, process_pid)

    def memgraph_file_name(self, process_name, process_pid):
        return "%s-%s.memgraph" % (process_name, process_pid)

    def count_total_bytes_and_unique_leaks(self, leak_files):
        merge_depth = 5  # ORWT had a --merge-leak-depth argument, but that seems out of scope for the run-webkit-tests tool.
        args = [
            '--merge-depth',
            merge_depth,
        ] + leak_files
        try:
            parse_malloc_history_output = self._port._run_script("parse-malloc-history", args, include_configuration_arguments=False)
        except ScriptError as e:
            _log.warn("Failed to parse leaks output: %s" % e.message_with_output())
            return

        # total: 5,888 bytes (0 bytes excluded).
        unique_leak_count = len(re.findall(r'^(\d*)\scalls', parse_malloc_history_output, re.MULTILINE))
        total_bytes_string = re.search(r'^total\:\s(.+)\s\(', parse_malloc_history_output, re.MULTILINE).group(1)
        return (total_bytes_string, unique_leak_count)

    def count_total_leaks(self, leak_file_paths):
        total_leaks = 0
        for leak_file_path in leak_file_paths:
            # Leaks have been seen to include non-utf8 data, so we use read_binary_file.
            # See https://bugs.webkit.org/show_bug.cgi?id=71112.
            leaks_output = self._filesystem.read_binary_file(leak_file_path)
            count, _, _ = self._parse_leaks_output(leaks_output)
            total_leaks += count
        return total_leaks

    def check_for_leaks(self, process_name, process_id):
        _log.debug("Checking for leaks in %s" % process_name)
        try:
            leaks_filename = self.leaks_file_name(process_name, process_id)
            leaks_output_path = self._filesystem.join(self._port.results_directory(), leaks_filename)
            # Oddly enough, run-leaks (or the underlying leaks tool) does not seem to always output utf-8,
            # thus we pass decode_output=False.  Without this code we've seen errors like:
            # "UnicodeDecodeError: 'utf8' codec can't decode byte 0x88 in position 779874: unexpected code byte"
            self._port._run_script("run-leaks", self._leaks_args(process_name, process_id), include_configuration_arguments=False, decode_output=False)
            leaks_output = self._filesystem.read_binary_file(leaks_output_path)
        except ScriptError as e:
            _log.warn("Failed to run leaks tool: %s" % e.message_with_output())
            return

        # FIXME: We end up parsing this output 3 times.  Once here and twice for summarizing.
        count, excluded, bytes = self._parse_leaks_output(leaks_output)
        adjusted_count = count - excluded
        if not adjusted_count:
            self._filesystem.remove(leaks_output_path)
            return

        # FIXME: Ideally we would not be logging from the worker process, but rather pass the leak
        # information back to the manager and have it log.
        if excluded:
            _log.info("%s leaks (%s bytes including %s excluded leaks) were found, details in %s" % (adjusted_count, bytes, excluded, leaks_output_path))
        else:
            _log.info("%s leaks (%s bytes) were found, details in %s" % (count, bytes, leaks_output_path))

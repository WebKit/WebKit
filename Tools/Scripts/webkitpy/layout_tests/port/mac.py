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

"""WebKit Mac implementation of the Port interface."""

import logging
import platform
import re

from webkitpy.common.system.executive import ScriptError
from webkitpy.layout_tests.port.webkit import WebKitPort


_log = logging.getLogger(__name__)


# If other ports/platforms decide to support --leaks, we should see about sharing as much of this code as possible.
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
    def _types_to_exlude_from_leaks(self):
        # Currently we don't have any type excludes from OS leaks, but we will likely again in the future.
        return []

    def _callstacks_to_exclude_from_leaks(self):
        callstacks = [
            "Flash_EnforceLocalSecurity",  # leaks in Flash plug-in code, rdar://problem/4449747
        ]
        if self._port.is_leopard():
            callstacks += [
                "CFHTTPMessageAppendBytes",  # leak in CFNetwork, rdar://problem/5435912
                "sendDidReceiveDataCallback",  # leak in CFNetwork, rdar://problem/5441619
                "_CFHTTPReadStreamReadMark",  # leak in CFNetwork, rdar://problem/5441468
                "httpProtocolStart",  # leak in CFNetwork, rdar://problem/5468837
                "_CFURLConnectionSendCallbacks",  # leak in CFNetwork, rdar://problem/5441600
                "DispatchQTMsg",  # leak in QuickTime, PPC only, rdar://problem/5667132
                "QTMovieContentView createVisualContext",  # leak in QuickTime, PPC only, rdar://problem/5667132
                "_CopyArchitecturesForJVMVersion",  # leak in Java, rdar://problem/5910823
            ]
        elif self._port.is_snowleopard():
            callstacks += [
                "readMakerNoteProps",  # <rdar://problem/7156432> leak in ImageIO
                "QTKitMovieControllerView completeUISetup",  # <rdar://problem/7155156> leak in QTKit
                "getVMInitArgs",  # <rdar://problem/7714444> leak in Java
                "Java_java_lang_System_initProperties",  # <rdar://problem/7714465> leak in Java
                "glrCompExecuteKernel",  # <rdar://problem/7815391> leak in graphics driver while using OpenGL
                "NSNumberFormatter getObjectValue:forString:errorDescription:",  # <rdar://problem/7149350> Leak in NSNumberFormatter
            ]
        return callstacks

    def _leaks_args(self, pid):
        leaks_args = []
        for callstack in self._callstacks_to_exclude_from_leaks():
            leaks_args += ['--exclude-callstack="%s"' % callstack]  # Callstacks can have spaces in them, so we quote the arg to prevent confusing perl's optparse.
        for excluded_type in self._types_to_exlude_from_leaks():
            leaks_args += ['--exclude-type="%s"' % excluded_type]
        leaks_args.append(pid)
        return leaks_args

    def _parse_leaks_output(self, leaks_output, process_pid):
        count, bytes = re.search(r'Process %s: (\d+) leaks? for (\d+) total' % process_pid, leaks_output).groups()
        excluded_match = re.search(r'(\d+) leaks? excluded', leaks_output)
        excluded = excluded_match.group(0) if excluded_match else 0
        return int(count), int(excluded), int(bytes)

    def leaks_files_in_directory(self, directory):
        return self._filesystem.glob(self._filesystem.join(directory, "leaks-*"))

    def leaks_file_name(self, process_name, process_pid):
        # We include the number of files this worker has already written in the name to prevent overwritting previous leak results..
        return "leaks-%s-%s.txt" % (process_name, process_pid)

    def parse_leak_files(self, leak_files):
        merge_depth = 5  # ORWT had a --merge-leak-depth argument, but that seems out of scope for the run-webkit-tests tool.
        args = [
            '--merge-depth',
            merge_depth,
        ] + leak_files
        parse_malloc_history_output = self._port._run_script("parse-malloc-history", args, include_configuration_arguments=False)

        unique_leak_count = len(re.findall(r'^(\d*)\scalls', parse_malloc_history_output))
        total_bytes = int(re.search(r'^total\:\s(.*)\s\(', parse_malloc_history_output).group(1))
        return (total_bytes, unique_leak_count)

    def check_for_leaks(self, process_name, process_pid):
        _log.debug("Checking for leaks in %s" % process_name)
        try:
            # Oddly enough, run-leaks (or the underlying leaks tool) does not seem to always output utf-8,
            # thus we pass decode_output=False.  Without this code we've seen errors like:
            # "UnicodeDecodeError: 'utf8' codec can't decode byte 0x88 in position 779874: unexpected code byte"
            leaks_output = self._port._run_script("run-leaks", self._leaks_args(process_pid), include_configuration_arguments=False, decode_output=False)
        except ScriptError, e:
            _log.warn("Failed to run leaks tool: %s" % e.message_with_output())
            return

        count, excluded, bytes = self._parse_leaks_output(leaks_output, process_pid)
        adjusted_count = count - excluded
        if not adjusted_count:
            return

        leaks_filename = self.leaks_file_name(process_name, process_pid)
        leaks_output_path = self._filesystem.join(self._port.results_directory(), leaks_filename)
        self._filesystem.write_binary_file(leaks_output_path, leaks_output)

        # FIXME: Ideally we would not be logging from the worker process, but rather pass the leak
        # information back to the manager and have it log.
        if excluded:
            _log.info("%s leaks (%s bytes including %s excluded leaks) were found, details in %s" % (adjusted_count, bytes, excluded, leaks_output_path))
        else:
            _log.info("%s leaks (%s bytes) were found, details in %s" % (count, bytes, leaks_output_path))


def os_version(os_version_string=None, supported_versions=None):
    if not os_version_string:
        if hasattr(platform, 'mac_ver') and platform.mac_ver()[0]:
            os_version_string = platform.mac_ver()[0]
        else:
            # Make up something for testing.
            os_version_string = "10.5.6"
    release_version = int(os_version_string.split('.')[1])
    version_strings = {
        5: 'leopard',
        6: 'snowleopard',
        # Add 7: 'lion' here?
    }
    assert release_version >= min(version_strings.keys())
    version_string = version_strings.get(release_version, 'future')
    if supported_versions:
        assert version_string in supported_versions
    return version_string


class MacPort(WebKitPort):
    port_name = "mac"

    # FIXME: 'wk2' probably shouldn't be a version, it should probably be
    # a modifier, like 'chromium-gpu' is to 'chromium'.
    SUPPORTED_VERSIONS = ('leopard', 'snowleopard', 'future', 'wk2')

    FALLBACK_PATHS = {
        'leopard': [
            'mac-leopard',
            'mac-snowleopard',
            'mac',
        ],
        'snowleopard': [
            'mac-snowleopard',
            'mac',
        ],
        'future': [
            'mac',
        ],
        'wk2': [],  # wk2 does not make sense as a version, this is only here to make the rebaseline unit tests not crash.
    }

    def __init__(self, port_name=None, os_version_string=None, **kwargs):
        port_name = port_name or 'mac'
        WebKitPort.__init__(self, port_name=port_name, **kwargs)
        if port_name == 'mac':
            self._version = os_version(os_version_string)
            self._name = port_name + '-' + self._version
        else:
            assert port_name.startswith('mac')
            self._version = port_name[len('mac-'):]
            assert self._version in self.SUPPORTED_VERSIONS, "%s is not in %s" % (self._version, self.SUPPORTED_VERSIONS)
        self._operating_system = 'mac'
        self._leak_detector = LeakDetector(self)
        if self.get_option("leaks"):
            # DumpRenderTree slows down noticably if we run more than about 1000 tests in a batch
            # with MallocStackLogging enabled.
            self.set_option_default("batch_size", 1000)

    def baseline_search_path(self):
        search_paths = self.FALLBACK_PATHS[self._version]
        if self.get_option('webkit_test_runner'):
            search_paths.insert(0, self._wk2_port_name())
        return map(self._webkit_baseline_path, search_paths)

    def setup_environ_for_server(self, server_name=None):
        env = WebKitPort.setup_environ_for_server(self, server_name)
        if server_name == self.driver_name():
            if self.get_option('leaks'):
                env['MallocStackLogging'] = '1'
            if self.get_option('guard_malloc'):
                env['DYLD_INSERT_LIBRARIES'] = '/usr/lib/libgmalloc.dylib'
        return env

    # Belongs on a Platform object.
    def is_leopard(self):
        return self._version == "leopard"

    # Belongs on a Platform object.
    def is_snowleopard(self):
        return self._version == "snowleopard"

    # Belongs on a Platform object.
    def is_crash_reporter(self, process_name):
        return re.search(r'ReportCrash', process_name)

    def _build_java_test_support(self):
        java_tests_path = self._filesystem.join(self.layout_tests_dir(), "java")
        build_java = ["/usr/bin/make", "-C", java_tests_path]
        if self._executive.run_command(build_java, return_exit_code=True):  # Paths are absolute, so we don't need to set a cwd.
            _log.error("Failed to build Java support files: %s" % build_java)
            return False
        return True

    def check_for_leaks(self, process_name, process_pid):
        if not self.get_option('leaks'):
            return
        # We could use http://code.google.com/p/psutil/ to get the process_name from the pid.
        self._leak_detector.check_for_leaks(process_name, process_pid)

    def print_leaks_summary(self):
        if not self.get_option('leaks'):
            return
        # We're in the manager process, so the leak detector will not have a valid list of leak files.
        # FIXME: This is a hack, but we don't have a better way to get this information from the workers yet.
        leaks_files = self._leak_detector.leaks_files_in_directory(self.results_directory())
        if not leaks_files:
            return
        total_bytes, unique_leaks = self._leak_detector.parse_leak_files(leaks_files)
        _log.info("%s total leaks found for a total of %s!" % (self._total_leaks, total_bytes))
        _log.info("%s unique leaks found!" % unique_leaks)

    def _check_port_build(self):
        return self._build_java_test_support()

    def _path_to_webcore_library(self):
        return self._build_path('WebCore.framework/Versions/A/WebCore')

    def show_results_html_file(self, results_filename):
        self._run_script('run-safari', ['-NSOpen', results_filename])

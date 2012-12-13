# Copyright (C) 2012 Google Inc. All rights reserved.
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

_log = logging.getLogger(__name__)


class ProfilerFactory(object):
    @classmethod
    def create_profiler(cls, host, executable_path, output_dir, profiler_name=None, identifier=None):
        profilers_by_name = cls.available_profilers_by_name(host.platform)
        profiler_class = profilers_by_name.get(profiler_name or cls.default_profiler_name(host.platform))
        if not profiler_class:
            return None
        return profiler_class(host, executable_path, output_dir, identifier)

    @classmethod
    def available_profilers_by_name(cls, platform):
        profilers = {'pprof': GooglePProf}
        if platform.is_mac():
            profilers['iprofiler'] = IProfiler
            profilers['sample'] = Sample
        return profilers

    @classmethod
    def default_profiler_name(cls, platform):
        if platform.is_mac():
            return 'iprofiler'
        if platform.is_linux():
            return 'pprof'
        return None


class Profiler(object):
    def __init__(self, host, executable_path, output_dir, identifier=None):
        self._host = host
        self._executable_path = executable_path
        self._output_dir = output_dir
        self._identifier = "test"
        self._host.filesystem.maybe_make_directory(self._output_dir)

    def adjusted_environment(self, env):
        return env

    def attach_to_pid(self, pid):
        pass

    def profile_after_exit(self):
        pass


class SingleFileOutputProfiler(Profiler):
    def __init__(self, host, executable_path, output_dir, output_suffix, identifier=None):
        super(SingleFileOutputProfiler, self).__init__(host, executable_path, output_dir, identifier)
        self._output_path = self._host.workspace.find_unused_filename(self._output_dir, self._identifier, output_suffix)


class GooglePProf(SingleFileOutputProfiler):
    def __init__(self, host, executable_path, output_dir, identifier=None):
        super(GooglePProf, self).__init__(host, executable_path, output_dir, "pprof", identifier)

    def adjusted_environment(self, env):
        env['CPUPROFILE'] = self._output_path
        return env

    def _first_ten_lines_of_profile(self, pprof_output):
        match = re.search("^Total:[^\n]*\n((?:[^\n]*\n){0,10})", pprof_output, re.MULTILINE)
        return match.group(1) if match else None

    def _pprof_path(self):
        # FIXME: We should have code to find the right google-pprof executable, some Googlers have
        # google-pprof installed as "pprof" on their machines for them.
        return '/usr/bin/google-pprof'

    def profile_after_exit(self):
        # google-pprof doesn't check its arguments, so we have to.
        if not (self._host.filesystem.exists(self._output_path)):
            print "Failed to gather profile, %s does not exist." % self._output_path
            return

        pprof_args = [self._pprof_path(), '--text', self._executable_path, self._output_path]
        profile_text = self._host.executive.run_command(pprof_args)
        print "First 10 lines of pprof --text:"
        print self._first_ten_lines_of_profile(profile_text)
        print "http://google-perftools.googlecode.com/svn/trunk/doc/cpuprofile.html documents output."
        print
        print "To interact with the the full profile, including produce graphs:"
        print ' '.join([self._pprof_path(), self._executable_path, self._output_path])


class Sample(SingleFileOutputProfiler):
    def __init__(self, host, executable_path, output_dir, identifier=None):
        super(Sample, self).__init__(host, executable_path, output_dir, "txt", identifier)
        self._profiler_process = None

    def attach_to_pid(self, pid):
        fs = self._host.filesystem
        cmd = ["sample", pid, "-mayDie", "-file", self._output_path]
        self._profiler_process = self._host.executive.popen(cmd)

    def profile_after_exit(self):
        self._profiler_process.wait()


class IProfiler(SingleFileOutputProfiler):
    def __init__(self, host, executable_path, output_dir, identifier=None):
        super(IProfiler, self).__init__(host, executable_path, output_dir, "dtps", identifier)
        self._profiler_process = None

    def attach_to_pid(self, pid):
        # FIXME: iprofiler requires us to pass the directory separately
        # from the basename of the file, with no control over the extension.
        fs = self._host.filesystem
        cmd = ["iprofiler", "-timeprofiler", "-a", pid,
                "-d", fs.dirname(self._output_path), "-o", fs.splitext(fs.basename(self._output_path))[0]]
        # FIXME: Consider capturing instead of letting instruments spam to stderr directly.
        self._profiler_process = self._host.executive.popen(cmd)

    def profile_after_exit(self):
        # It seems like a nicer user experiance to wait on the profiler to exit to prevent
        # it from spewing to stderr at odd times.
        self._profiler_process.wait()

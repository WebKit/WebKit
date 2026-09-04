# Copyright (C) 2026 Apple Inc. All rights reserved.
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

import json
import os
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

SCRIPT = str(Path(__file__).resolve().parents[2] / 'measure-build-time')


class MeasureBuildTimeTest(unittest.TestCase):
    def _run_script(self, build_command, extra_args=None):
        with tempfile.TemporaryDirectory() as tmpdir:
            build_dir = os.path.join(tmpdir, 'Build')
            os.makedirs(build_dir)
            output_file = os.path.join(tmpdir, 'results.json')

            cmd = [
                sys.executable, SCRIPT,
                '--build-command', build_command,
                '--build-dir', build_dir,
                '--source-dir', tmpdir,
                '--output', output_file,
            ]
            if extra_args:
                cmd.extend(extra_args)

            proc = subprocess.run(cmd, stdin=subprocess.DEVNULL, cwd=tmpdir,
                                  capture_output=True, text=True)
            results = None
            if os.path.exists(output_file):
                with open(output_file) as f:
                    results = json.load(f)
            return proc, results

    def test_clean_and_null_build(self):
        proc, results = self._run_script('echo ok', ['--tests', 'clean', 'null'])

        self.assertEqual(proc.returncode, 0)
        self.assertIn('BuildTime-Debug', results)
        tests = results['BuildTime-Debug']['tests']
        self.assertIn('clean', tests)
        self.assertIn('null', tests)
        for name in ('clean', 'null'):
            time_values = tests[name]['metrics']['Time']['current']
            self.assertEqual(len(time_values), 1)
            self.assertIsInstance(time_values[0], int)
            self.assertGreaterEqual(time_values[0], 0)

    def test_timing_summary_parsing(self):
        timing_output = textwrap.dedent('''\
            Build Timing Summary
            CompileC (5 tasks) | 12.345 seconds
            CompileC++ (3 tasks) | 4.567 seconds
            Ld (1 task) | 2.100 seconds
            ** BUILD SUCCEEDED **
        ''')
        build_command = "cat <<'ENDOFBUILD'\n" + timing_output + "ENDOFBUILD\n"
        proc, results = self._run_script(build_command, ['--tests', 'clean'])

        self.assertEqual(proc.returncode, 0)
        clean = results['BuildTime-Debug']['tests']['clean']
        phases = clean['tests']['Phases']

        self.assertIn('CPUTime', phases['metrics'])
        cpu_time = phases['metrics']['CPUTime']['current'][0]
        self.assertEqual(cpu_time, 12345 + 4567 + 2100)

        phase_tests = phases['tests']
        self.assertIn('CompileC', phase_tests)
        self.assertIn('CompileCPlusPlus', phase_tests)
        self.assertIn('Ld', phase_tests)
        self.assertEqual(phase_tests['CompileC']['metrics']['Time']['current'], [12345])
        self.assertEqual(phase_tests['CompileCPlusPlus']['metrics']['Time']['current'], [4567])
        self.assertEqual(phase_tests['Ld']['metrics']['Time']['current'], [2100])

    def test_build_failure(self):
        proc, results = self._run_script('exit 1')

        self.assertNotEqual(proc.returncode, 0)
        self.assertIsNone(results)

    def test_single_test_selection(self):
        proc, results = self._run_script('echo ok', ['--tests', 'null'])

        self.assertEqual(proc.returncode, 0)
        tests = results['BuildTime-Debug']['tests']
        self.assertIn('null', tests)
        self.assertNotIn('clean', tests)

    def test_custom_metric_key(self):
        proc, results = self._run_script('echo ok', ['--tests', 'null', '--metric-key', 'BuildTime-Release'])

        self.assertEqual(proc.returncode, 0)
        self.assertIn('BuildTime-Release', results)
        self.assertNotIn('BuildTime-Debug', results)


# The trace finalizer CMake generates when configured with -DSWIFT_NINJA_TRACE=ON.
# The stub records its argv and produces the file it was asked for.
TRACE_FINALIZE_STUB = textwrap.dedent('''\
    #!/bin/sh
    echo "$@" >> "$(dirname "$0")/finalize-argv.txt"
    while [ $# -gt 1 ]; do shift; done
    : > "$1"
''')


class MeasureBuildTimeTraceTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.tmpdir = Path(directory.name)
        self.build_dir = self.tmpdir / 'Build'
        self.build_dir.mkdir()
        self.traces = self.tmpdir / 'build-time-traces'

        self.stub = self.tmpdir / 'finalize-stub.sh'
        self.stub.write_text(TRACE_FINALIZE_STUB)
        self.stub.chmod(0o755)

    @property
    def finalizer(self):
        return self.build_dir / 'swift-trace-finalize.sh'

    def _run_script(self, tests, extra_args=()):
        cmd = [
            sys.executable, SCRIPT,
            '--build-command', 'echo ok',
            '--build-dir', str(self.build_dir),
            '--source-dir', str(self.tmpdir),
            '--output', str(self.tmpdir / 'results.json'),
            '--tests', *tests,
            *extra_args,
        ]
        return subprocess.run(cmd, stdin=subprocess.DEVNULL, cwd=self.tmpdir,
                              capture_output=True, text=True)

    def test_a_trace_is_written_for_each_test(self):
        # The clean test deletes the build directory, so the finalizer arrives the
        # way CMake writes it: at configure time.
        proc = self._run_script(['clean', 'null'], [
            '--trace',
            '--configure-command',
            f'mkdir -p {self.build_dir} && cp {self.stub} {self.finalizer}',
        ])
        self.assertEqual(proc.returncode, 0, proc.stderr)

        self.assertEqual(sorted(p.name for p in self.traces.iterdir()),
                         ['clean.json.gz', 'null.json.gz'])

        invocations = (self.build_dir / 'finalize-argv.txt').read_text().splitlines()
        self.assertEqual(len(invocations), 2)
        for argv in invocations:
            # The build's logs accumulate across builds, so each trace has to be
            # bounded to the ninja invocation this test just ran.
            self.assertIn('--since-epoch-ms=', argv)

    def test_stale_traces_do_not_survive_a_later_run(self):
        self.traces.mkdir()
        (self.traces / 'webcore-header.json.gz').write_text('stale')

        self._run_script(['null'], ['--trace'])
        self.assertEqual(list(self.traces.iterdir()), [])

    def test_an_untraced_build_directory_only_warns(self):
        proc = self._run_script(['null'], ['--trace'])
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn('SWIFT_NINJA_TRACE', proc.stderr)
        self.assertFalse(self.traces.exists())

    def test_tracing_is_off_without_cmake(self):
        shutil.copy(self.stub, self.finalizer)
        proc = self._run_script(['null'])
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertFalse((self.build_dir / 'finalize-argv.txt').exists())

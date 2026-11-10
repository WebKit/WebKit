# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ANALYZE_SAFER_CPP = os.path.realpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'analyze-safer-cpp'))


@unittest.skipIf(sys.platform.startswith('win'), 'fake clang stub is a POSIX shell script')
class AnalyzeSaferCppTest(unittest.TestCase):
    def test_compile_failure_is_not_reported_as_a_clean_pass(self):
        # A file that fails to compile (e.g. a fatal-error missing header) must not be
        # reported as analyzed cleanly: clang's non-zero exit has to fail the run, even
        # though a "fatal error:" line is not one of the checker diagnostics the tool parses.
        with tempfile.TemporaryDirectory() as directory:
            fakeClang = os.path.join(directory, 'fake-clang')
            with open(fakeClang, 'w') as f:
                f.write('#!/bin/sh\n'
                        'echo "$0:1:10: fatal error: \'nope.h\' file not found" 1>&2\n'
                        'exit 1\n')
            os.chmod(fakeClang, 0o755)

            source = os.path.join(directory, 'repro.cpp')
            with open(source, 'w') as f:
                f.write('#include "nope.h"\nint main() { return 0; }\n')

            compileCommands = os.path.join(directory, 'compile_commands.json')
            with open(compileCommands, 'w') as f:
                json.dump([{'directory': directory, 'file': source,
                            'command': 'clang++ -c {} -o {}.o'.format(source, source)}], f)

            proc = subprocess.run(
                [sys.executable, ANALYZE_SAFER_CPP, source,
                 '--compile-commands', compileCommands, '--clang', fakeClang],
                capture_output=True, text=True)

            output = proc.stdout + proc.stderr
            self.assertNotEqual(proc.returncode, 0,
                                'a compile failure must not exit 0:\n' + output)
            self.assertIn('did not compile', output)


def write_fake_clang(path, checkers):
    # Answers `-cc1 -analyzer-checker-help` with the given checkers, and exits
    # cleanly with no diagnostics for any other invocation.
    with open(path, 'w') as f:
        f.write('#!/bin/sh\n'
                'case "$*" in *-analyzer-checker-help*)\n')
        for checker in checkers:
            f.write('    echo "  {}"\n'.format(checker))
        f.write('    ;;\n'
                'esac\n'
                'exit 0\n')
    os.chmod(path, 0o755)


@unittest.skipIf(sys.platform.startswith('win') or sys.platform == 'darwin', 'Linux analyzer discovery')
class AnalyzeSaferCppLinuxTest(unittest.TestCase):
    def test_picks_the_clang_with_the_most_checkers(self):
        with tempfile.TemporaryDirectory() as directory:
            binDir = os.path.join(directory, 'bin')
            os.mkdir(binDir)
            write_fake_clang(os.path.join(binDir, 'clang++'), ['webkit.NoUncountedMemberChecker'])
            write_fake_clang(os.path.join(binDir, 'clang++-23'), ['webkit.NoUncountedMemberChecker', 'alpha.webkit.UncountedCallArgsChecker'])
            write_fake_clang(os.path.join(binDir, 'clang++-22'), ['webkit.NoUncountedMemberChecker'])
            write_fake_clang(os.path.join(binDir, 'g++'), [])

            source = os.path.join(directory, 'repro.cpp')
            with open(source, 'w') as f:
                f.write('int main() { return 0; }\n')

            compileCommands = os.path.join(directory, 'compile_commands.json')
            with open(compileCommands, 'w') as f:
                json.dump([{'directory': directory, 'file': source,
                            'command': 'g++ -c {} -o {}.o'.format(source, source)}], f)

            # Only the fake compilers (and git) are visible, so a real clang in PATH cannot win.
            git = shutil.which('git')
            if git:
                os.symlink(git, os.path.join(binDir, 'git'))
            env = dict(os.environ, PATH=binDir)
            env.pop('CXX', None)
            proc = subprocess.run(
                [sys.executable, ANALYZE_SAFER_CPP, source, '--compile-commands', compileCommands],
                capture_output=True, text=True, env=env)

            output = proc.stdout + proc.stderr
            self.assertEqual(proc.returncode, 0, output)
            self.assertIn('Using analyzer: {}'.format(os.path.join(binDir, 'clang++-23')), output)


@unittest.skipIf(sys.platform.startswith('win'), 'fake clang stub is a POSIX shell script')
class AnalyzeSaferCppCheckerOptionTest(unittest.TestCase):
    def test_unlisted_alpha_webkit_checker_is_passed_through(self):
        # A checker the registry does not know, such as a borrow checker, can still be requested by its full name.
        with tempfile.TemporaryDirectory() as directory:
            fakeClang = os.path.join(directory, 'fake-clang')
            with open(fakeClang, 'w') as f:
                f.write('#!/bin/sh\n'
                        'case "$*" in *-analyzer-checker-help*) echo "  alpha.webkit.UnborrowedCallArgsChecker"; exit 0;; esac\n'
                        'exit 0\n')
            os.chmod(fakeClang, 0o755)
            source = os.path.join(directory, 'repro.cpp')
            with open(source, 'w') as f:
                f.write('int main() { return 0; }\n')
            compileCommands = os.path.join(directory, 'compile_commands.json')
            with open(compileCommands, 'w') as f:
                json.dump([{'directory': directory, 'file': source,
                            'command': 'clang++ -c {} -o {}.o'.format(source, source)}], f)

            proc = subprocess.run(
                [sys.executable, ANALYZE_SAFER_CPP, source, '--compile-commands', compileCommands, '--clang', fakeClang,
                 '-v', '--checker', 'alpha.webkit.UnborrowedCallArgsChecker'],
                capture_output=True, text=True)
            self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
            self.assertIn('-analyzer-checker -Xclang alpha.webkit.UnborrowedCallArgsChecker ', proc.stderr)

            proc = subprocess.run(
                [sys.executable, ANALYZE_SAFER_CPP, source, '--compile-commands', compileCommands, '--clang', fakeClang,
                 '--checker', 'bogus.Checker'],
                capture_output=True, text=True)
            self.assertNotEqual(proc.returncode, 0)
            self.assertIn("unknown checker 'bogus.Checker'", proc.stderr)


def write_empty_clang(path):
    with open(path, 'w') as f:
        f.write('#!/bin/sh\nexit 0\n')
    os.chmod(path, 0o755)


@unittest.skipIf(sys.platform.startswith('win'), 'fake clang stub is a POSIX shell script')
class AnalyzeSaferCppHeaderFlagsTest(unittest.TestCase):
    def test_header_borrows_flags_from_a_sibling_in_a_unified_bundle(self):
        webkitRoot = os.path.dirname(os.path.dirname(os.path.dirname(ANALYZE_SAFER_CPP)))
        header = os.path.join(webkitRoot, 'Source', 'WebCore', 'rendering', 'PaintInfo.h')
        thirdParty = os.path.join(webkitRoot, 'Source', 'WebCore', 'PAL', 'ThirdParty', 'dav1d', 'src', 'cdf.c')
        with tempfile.TemporaryDirectory() as directory:
            fakeClang = os.path.join(directory, 'fake-clang')
            write_empty_clang(fakeClang)
            bundleDir = os.path.join(directory, 'unified-sources')
            os.mkdir(bundleDir)
            bundle = os.path.join(bundleDir, 'UnifiedSource1.cpp')
            with open(bundle, 'w') as f:
                f.write('#include "rendering/RenderBox.cpp"\n')
            compileCommands = os.path.join(directory, 'compile_commands.json')
            with open(compileCommands, 'w') as f:
                json.dump([{'directory': directory, 'file': path, 'command': 'clang++ -c {} -o {}.o'.format(path, path)}
                           for path in (thirdParty, bundle)], f)

            proc = subprocess.run(
                [sys.executable, ANALYZE_SAFER_CPP, header, '--compile-commands', compileCommands, '--clang', fakeClang, '-v'],
                capture_output=True, text=True)
            self.assertIn('/unified-sources/UnifiedSource1.cpp)', proc.stderr)
            self.assertNotIn('cdf.c', proc.stderr)


@unittest.skipIf(sys.platform.startswith('win'), 'fake clang stub is a POSIX shell script')
class AnalyzeSaferCppUnifiedBundleTest(unittest.TestCase):
    def test_source_in_a_prefixed_unified_bundle_is_analyzed(self):
        webkitRoot = os.path.dirname(os.path.dirname(os.path.dirname(ANALYZE_SAFER_CPP)))
        source = os.path.join(webkitRoot, 'Source', 'WebKit', 'Platform', 'IPC', 'Connection.cpp')
        with tempfile.TemporaryDirectory() as directory:
            fakeClang = os.path.join(directory, 'fake-clang')
            write_empty_clang(fakeClang)
            bundleDir = os.path.join(directory, 'unified-sources')
            os.mkdir(bundleDir)
            bundle = os.path.join(bundleDir, 'PlatformUnifiedSource-Platform-1.cpp')
            with open(bundle, 'w') as f:
                f.write('#include "Platform/IPC/Connection.cpp"\n')
            compileCommands = os.path.join(directory, 'compile_commands.json')
            with open(compileCommands, 'w') as f:
                json.dump([{'directory': directory, 'file': bundle, 'command': 'clang++ -c {} -o {}.o'.format(bundle, bundle)}], f)

            proc = subprocess.run(
                [sys.executable, ANALYZE_SAFER_CPP, source, '--compile-commands', compileCommands, '--clang', fakeClang, '--no-prepare'],
                capture_output=True, text=True)
            self.assertNotIn('SKIP', proc.stdout)
            self.assertIn('1 file(s) analyzed', proc.stdout)


@unittest.skipIf(sys.platform.startswith('win'), 'fake ninja stub is a POSIX shell script')
class AnalyzeSaferCppPrepareTest(unittest.TestCase):
    def run_with_fake_ninja(self, directory, ninjaExitCode, extraArgs=(), otherTargets=()):
        fakeClang = os.path.join(directory, 'fake-clang')
        write_empty_clang(fakeClang)
        log = os.path.join(directory, 'ninja.log')
        fakeNinja = os.path.join(directory, 'fake-ninja')
        with open(fakeNinja, 'w') as f:
            f.write('#!/bin/sh\necho "$@" >> {}\nexit {}\n'.format(log, ninjaExitCode))
        os.chmod(fakeNinja, 0o755)
        with open(os.path.join(directory, 'CMakeCache.txt'), 'w') as f:
            f.write('CMAKE_MAKE_PROGRAM:FILEPATH={}\n'.format(fakeNinja))
        open(os.path.join(directory, 'build.ninja'), 'w').close()
        source = os.path.join(directory, 'repro.cpp')
        with open(source, 'w') as f:
            f.write('int main() { return 0; }\n')
        compileCommands = os.path.join(directory, 'compile_commands.json')
        entries = []
        for target in ('ReproSub',) + tuple(otherTargets):
            path = source if target == 'ReproSub' else os.path.join(directory, target + '.cpp')
            output = 'CMakeFiles/{}.dir/{}.o'.format(target, os.path.basename(path))
            entries.append({'directory': directory, 'file': path, 'output': output,
                            'command': 'clang++ -c {} -o {}'.format(path, output)})
        with open(compileCommands, 'w') as f:
            json.dump(entries, f)
        proc = subprocess.run(
            [sys.executable, ANALYZE_SAFER_CPP, source, '--compile-commands', compileCommands, '--clang', fakeClang] + list(extraArgs),
            capture_output=True, text=True)
        ninjaCalls = open(log).read().splitlines() if os.path.exists(log) else []
        return proc, ninjaCalls

    def test_reconfigures_and_updates_the_targets_of_analyzed_files(self):
        with tempfile.TemporaryDirectory() as directory:
            proc, ninjaCalls = self.run_with_fake_ninja(directory, 0)
            self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
            self.assertEqual(ninjaCalls, ['build.ninja', 'cmake_object_order_depends_target_ReproSub'])

    def test_updates_the_parent_of_a_sub_target_but_not_other_siblings(self):
        with tempfile.TemporaryDirectory() as directory:
            proc, ninjaCalls = self.run_with_fake_ninja(directory, 0, otherTargets=('Repro', 'ReproTestSupport'))
            self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
            self.assertEqual(ninjaCalls, ['build.ninja', 'cmake_object_order_depends_target_Repro cmake_object_order_depends_target_ReproSub'])

    def test_failed_update_stops_the_run(self):
        with tempfile.TemporaryDirectory() as directory:
            proc, _ = self.run_with_fake_ninja(directory, 1)
            self.assertNotEqual(proc.returncode, 0)
            self.assertIn('--no-prepare', proc.stderr)

    def test_no_prepare_skips_ninja(self):
        with tempfile.TemporaryDirectory() as directory:
            proc, ninjaCalls = self.run_with_fake_ninja(directory, 1, ['--no-prepare'])
            self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
            self.assertEqual(ninjaCalls, [])


if __name__ == '__main__':
    unittest.main()

# Copyright (C) 2013 University of Szeged
# Copyright (C) 2013, 2026 Igalia S.L.
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
#     * Neither the name of the copyright holder nor the names of its
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

import builtins
import collections
import io
import os
import json
import re
import shutil
import resource
import tempfile
import threading
import time
import unittest
from unittest import mock

from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.port.linux_get_crash_log import CoreDumpMethod, CrashLogEnvVars, CrashLogUtils, GDBCrashLogGenerator, GDBCrashLogStartupHandler, LockFile, ThreadNamesCrashLogCapturer


# GDBCrashLogGenerator with determine_coredump_method_and_dir patched
def _make_generator(name='WebKitWebProcess', pid=12345, coredump_method=None, coredump_pattern='core-pid_%p.dump', coredump_directory='/tmp', has_variable_format_specifiers=False):
    if coredump_method is None:
        coredump_method = CoreDumpMethod.Abspath
    with mock.patch.object(CrashLogUtils, 'determine_coredump_method_and_dir', return_value=(coredump_method, coredump_pattern, coredump_directory, has_variable_format_specifiers)):
        return GDBCrashLogGenerator(executive=MockExecutive(), name=name, pid=pid, newer_than=None, filesystem=MockFileSystem(), path_to_driver=None, port_name='gtk', configuration='Debug')


class AreCoredumpsEnabledTest(unittest.TestCase):

    def test_coredumpctl_always_enabled(self):
        self.assertTrue(CrashLogUtils.are_coredumps_enabled(CoreDumpMethod.Coredumpctl))
        self.assertTrue(CrashLogUtils.are_coredumps_enabled_and_unlimited(CoreDumpMethod.Coredumpctl))

    def test_abspath_checks_rlimit(self):
        with mock.patch('resource.getrlimit', return_value=(0, 0)):
            self.assertFalse(CrashLogUtils.are_coredumps_enabled(CoreDumpMethod.Abspath))
        with mock.patch('resource.getrlimit', return_value=(1024, 1024)):
            self.assertTrue(CrashLogUtils.are_coredumps_enabled(CoreDumpMethod.Abspath))
            self.assertFalse(CrashLogUtils.are_coredumps_enabled_and_unlimited(CoreDumpMethod.Abspath))
        with mock.patch('resource.getrlimit', return_value=(resource.RLIM_INFINITY, resource.RLIM_INFINITY)):
            self.assertTrue(CrashLogUtils.are_coredumps_enabled_and_unlimited(CoreDumpMethod.Abspath))


class CorePatternToRegexTest(unittest.TestCase):

    def test_literal_string_passes_through(self):
        self.assertEqual(CrashLogUtils.core_pattern_to_regex('core'), 'core')

    def test_specifier_becomes_wildcard(self):
        # %p -> .+ (non-empty: the kernel always writes something)
        self.assertEqual(CrashLogUtils.core_pattern_to_regex('core-%p'), r'core\-.+')

    def test_named_pid_capture_group(self):
        regex = CrashLogUtils.core_pattern_to_regex('core-pid_%p.dump', generate_regex_for_pid_match=True)
        m = re.fullmatch(regex, 'core-pid_28529.dump')
        self.assertIsNotNone(m)
        self.assertEqual(m.group('pid'), '28529')

    def test_pid_capture_only_when_requested(self):
        without = CrashLogUtils.core_pattern_to_regex('core-%p')
        with_capture = CrashLogUtils.core_pattern_to_regex('core-%p', generate_regex_for_pid_match=True)
        self.assertNotIn('(?P<pid>', without)
        self.assertIn('(?P<pid>', with_capture)

    def test_double_percent_is_literal_percent(self):
        self.assertEqual(CrashLogUtils.core_pattern_to_regex('core%%'), 'core%')

    def test_trailing_lone_percent_is_dropped(self):
        # kernel rule: %<NUL> at end -> drop the %
        self.assertEqual(CrashLogUtils.core_pattern_to_regex('core%'), 'core')

    def test_invalid_specifier_is_dropped(self):
        # %z isn't valid -> kernel drops both characters silently
        self.assertEqual(CrashLogUtils.core_pattern_to_regex('core%z'), 'core')

    def test_regex_metacharacters_are_escaped(self):
        regex = CrashLogUtils.core_pattern_to_regex('core.dump')
        self.assertIsNotNone(re.fullmatch(regex, 'core.dump'))
        # The dot should be escaped, so 'X' should not match
        self.assertIsNone(re.fullmatch(regex, 'coreXdump'))

    def test_full_path_with_multiple_specifiers(self):
        regex = CrashLogUtils.core_pattern_to_regex('/var/tmp/core/core-%e-%p-%t', generate_regex_for_pid_match=True)
        m = re.fullmatch(regex, '/var/tmp/core/core-WebKit-1234-1700000000')
        self.assertIsNotNone(m)
        self.assertEqual(m.group('pid'), '1234')


class ExpandCorePatternTest(unittest.TestCase):

    def test_no_specifiers(self):
        expanded, has_var = CrashLogUtils.expand_core_pattern_and_detect_variable_specifiers('/var/tmp/core/foo')
        self.assertEqual(expanded, '/var/tmp/core/foo')
        self.assertFalse(has_var)

    def test_fixed_specifier_uid_is_expanded(self):
        expanded, has_var = CrashLogUtils.expand_core_pattern_and_detect_variable_specifiers('/var/tmp/core/u%u/')
        self.assertEqual(expanded, f'/var/tmp/core/u{os.getuid()}/')
        self.assertFalse(has_var)

    def test_variable_specifier_pid_is_detected(self):
        _, has_var = CrashLogUtils.expand_core_pattern_and_detect_variable_specifiers('/var/tmp/core/%p')
        self.assertTrue(has_var)

    def test_double_percent_preserved_as_literal(self):
        expanded, has_var = CrashLogUtils.expand_core_pattern_and_detect_variable_specifiers('/var/tmp/core/100%%')
        self.assertEqual(expanded, '/var/tmp/core/100%')
        self.assertFalse(has_var)

    def test_trailing_lone_percent_is_dropped(self):
        expanded, has_var = CrashLogUtils.expand_core_pattern_and_detect_variable_specifiers('/var/tmp/core/foo%')
        self.assertEqual(expanded, '/var/tmp/core/foo')
        self.assertFalse(has_var)


class HumanReadableSizeTest(unittest.TestCase):

    def _file_of_size(self, size):
        fd, path = tempfile.mkstemp()
        try:
            os.write(fd, b'a' * size)
        finally:
            os.close(fd)
        self.addCleanup(os.unlink, path)
        return path

    def test_small_file_reports_bytes(self):
        path = self._file_of_size(512)
        self.assertIn(' B', CrashLogUtils.human_readable_size(path))

    def test_few_kilobyte_file_reports_kb(self):
        path = self._file_of_size(4096)
        self.assertIn(' KB', CrashLogUtils.human_readable_size(path))

    def test_empty_file(self):
        path = self._file_of_size(0)
        self.assertEqual(CrashLogUtils.human_readable_size(path), '0.0 B')


class MakeTempPathTest(unittest.TestCase):

    def test_creates_a_real_empty_file(self):
        path = CrashLogUtils.make_temp_path()
        self.addCleanup(os.unlink, path)
        self.assertTrue(os.path.isfile(path))
        self.assertEqual(os.path.getsize(path), 0)

    def test_respects_suffix(self):
        path = CrashLogUtils.make_temp_path(suffix='.dump')
        self.addCleanup(os.unlink, path)
        self.assertTrue(path.endswith('.dump'))

    def test_respects_dir(self):
        with tempfile.TemporaryDirectory() as d:
            path = CrashLogUtils.make_temp_path(dir=d)
            self.assertEqual(os.path.dirname(os.path.realpath(path)), os.path.realpath(d))

    def test_paths_are_unique(self):
        p1 = CrashLogUtils.make_temp_path()
        p2 = CrashLogUtils.make_temp_path()
        self.addCleanup(os.unlink, p1)
        self.addCleanup(os.unlink, p2)
        self.assertNotEqual(p1, p2)


class SafeGetmtimeMostRecentExistingTest(unittest.TestCase):

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmpdir, True)

    def test_returns_mtime_for_existing_file(self):
        path = os.path.join(self.tmpdir, 'f')
        with open(path, 'w') as f:
            f.write('')
        os.utime(path, (1234, 1234))
        self.assertEqual(CrashLogUtils.safe_getmtime(path), 1234)

    def test_returns_none_for_missing_file(self):
        self.assertIsNone(CrashLogUtils.safe_getmtime(os.path.join(self.tmpdir, 'does-not-exist')))

    def _touch(self, name, mtime):
        path = os.path.join(self.tmpdir, name)
        with open(path, 'w') as f:
            f.write('')
        os.utime(path, (mtime, mtime))
        return path

    def test_empty_returns_none(self):
        self.assertIsNone(CrashLogUtils.most_recent_existing([]))

    def test_picks_newest(self):
        old = self._touch('a', 1000)
        new = self._touch('b', 2000)
        self.assertEqual(CrashLogUtils.most_recent_existing([old, new]), new)

    def test_skips_vanished_path(self):
        alive = self._touch('a', 1000)
        ghost = os.path.join(self.tmpdir, 'ghost')  # never created
        self.assertEqual(CrashLogUtils.most_recent_existing([alive, ghost]), alive)

    def test_newest_vanished_falls_back_to_next(self):
        alive = self._touch('a', 1000)
        newer = self._touch('b', 2000)
        os.remove(newer)  # the candidate we would have picked is gone
        self.assertEqual(CrashLogUtils.most_recent_existing([alive, newer]), alive)

    def test_all_vanished_returns_none(self):
        ghost1 = os.path.join(self.tmpdir, 'g1')
        ghost2 = os.path.join(self.tmpdir, 'g2')
        self.assertIsNone(CrashLogUtils.most_recent_existing([ghost1, ghost2]))


class LockFileTest(unittest.TestCase):

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmpdir, True)

    def test_acquire_creates_and_release_removes_lockfile(self):
        path = os.path.join(self.tmpdir, 'single.lock')
        lock = LockFile(path)
        lock.acquire()
        self.assertTrue(os.path.exists(path))
        lock.release()
        self.assertFalse(os.path.exists(path))

    def test_context_manager(self):
        path = os.path.join(self.tmpdir, 'cm.lock')
        with LockFile(path):
            self.assertTrue(os.path.exists(path))
        self.assertFalse(os.path.exists(path))

    def test_release_when_not_acquired_is_noop(self):
        lock = LockFile(os.path.join(self.tmpdir, 'never.lock'))
        lock.release()
        self.assertTrue(True)  # Must not raise

    def test_lockfile_contains_pid(self):
        path = os.path.join(self.tmpdir, 'pid.lock')
        with LockFile(path):
            with open(path) as f:
                self.assertEqual(f.read(), str(os.getpid()))

    def test_single_slot_serializes_two_threads(self):
        path = os.path.join(self.tmpdir, 'serialize.lock')
        events = []

        def worker(tag):
            with LockFile(path):
                events.append(('in', tag))
                time.sleep(0.05)
                events.append(('out', tag))

        t1 = threading.Thread(target=worker, args=('a',))
        t2 = threading.Thread(target=worker, args=('b',))
        t1.start()
        t2.start()
        t1.join()
        t2.join()

        # Must alternate in/out for the same tag without interleaving
        self.assertEqual(len(events), 4)
        self.assertEqual(events[0][0], 'in')
        self.assertEqual(events[1][0], 'out')
        self.assertEqual(events[0][1], events[1][1])
        self.assertEqual(events[2][0], 'in')
        self.assertEqual(events[3][0], 'out')
        self.assertEqual(events[2][1], events[3][1])

    def test_multi_slot_allows_concurrent_holders(self):
        path = os.path.join(self.tmpdir, 'multi.lock')
        lock1 = LockFile(path, slots=2)
        lock2 = LockFile(path, slots=2)
        lock1.acquire()
        try:
            # Should not block since slots=2
            lock2.acquire()
            try:
                self.assertNotEqual(lock1._locked_path, lock2._locked_path)
                self.assertTrue(os.path.exists(lock1._locked_path))
                self.assertTrue(os.path.exists(lock2._locked_path))
            finally:
                lock2.release()
        finally:
            lock1.release()

    def test_get_gdb_lock_returns_distinct_lock_instances(self):
        with mock.patch.object(CrashLogUtils, '_get_gdb_lock_configuration', return_value=('/tmp/gdb.lock', 1)):
            lock1 = CrashLogUtils.get_gdb_lock()
            lock2 = CrashLogUtils.get_gdb_lock()

        self.assertIsInstance(lock1, LockFile)
        self.assertIsInstance(lock2, LockFile)
        self.assertIsNot(lock1, lock2)
        self.assertEqual(lock1.path, lock2.path)


class GDBCrashLogGeneratorHelpersTest(unittest.TestCase):

    def setUp(self):
        self.gen = _make_generator(name='WebKitWebProcess', pid=12345)

    def test_pid_representation_known(self):
        self.assertEqual(self.gen._pid_representation(), '12345')

    def test_pid_representation_none(self):
        self.gen.pid = None
        self.assertEqual(self.gen._pid_representation(), '<unknown>')

    def test_pid_is_valid_when_numeric(self):
        self.assertTrue(self.gen._pid_is_valid())

    def test_pid_is_invalid_when_none(self):
        self.gen.pid = None
        self.assertFalse(self.gen._pid_is_valid())

    def test_pid_is_invalid_when_non_numeric_string(self):
        self.gen.pid = 'unknown'
        self.assertFalse(self.gen._pid_is_valid())

    def test_pattern_with_pid_specifier(self):
        self.assertTrue(self.gen._coredump_pattern_has_pid_format_string())

    def test_pattern_without_pid_specifier(self):
        self.gen._coredump_pattern = 'core'
        self.assertFalse(self.gen._coredump_pattern_has_pid_format_string())

    def test_underscore_header_basic(self):
        header = self.gen._underscore_header('SECTION')
        self.assertTrue(header.startswith('\n\n'))
        self.assertIn('SECTION', header)
        self.assertIn('-' * len('SECTION'), header)

    def test_underscore_header_first_omits_leading_newlines(self):
        header = self.gen._underscore_header('FIRST', first_header=True)
        self.assertFalse(header.startswith('\n\n'))
        self.assertIn('FIRST', header)


class BuildWarningMsgTest(unittest.TestCase):

    def setUp(self):
        self.gen = _make_generator(name='WebKitWebProcess', pid=999)

    def test_pid_found_returns_none(self):
        self.assertIsNone(self.gen._build_warning_msg(pid_found=True))

    def test_name_found_mentions_program_name(self):
        msg = self.gen._build_warning_msg(name_found=True)
        self.assertIn('WebKitWebProcess', msg)
        self.assertIn('matching the crashing program name', msg)

    def test_last_found_warns_high_risk(self):
        msg = self.gen._build_warning_msg(last_found=True)
        self.assertIn('high risk', msg)

    def test_nothing_found_default_path_suggests_env_var(self):
        # The default (env var unset) path of nothing_found should recommend the fallback env var by name.
        with mock.patch.object(CrashLogUtils, 'allow_unreliable_fallback_to_latest_coredump', return_value=False):
            msg = self.gen._build_warning_msg(nothing_found=True)
        self.assertIn(CrashLogEnvVars.allow_unreliable_fallback_to_latest_coredump, msg)

    def test_no_flag_set_raises(self):
        with self.assertRaises(RuntimeError):
            self.gen._build_warning_msg()


class PickMostRecentTest(unittest.TestCase):

    def setUp(self):
        self.gen = _make_generator(name='Foo', pid=100)
        self.tmpdir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmpdir, True)

    def _touch(self, name, mtime=None):
        path = os.path.join(self.tmpdir, name)
        with open(path, 'w') as f:
            f.write('')
        if mtime is not None:
            os.utime(path, (mtime, mtime))
        return path

    def test_empty_candidates_returns_none(self):
        self.assertIsNone(self.gen._pick_most_recent([], last_found=True))

    def test_no_pattern_match_returns_none(self):
        # File that doesn't match the core_pattern regex
        p = self._touch('not-a-core.txt')
        self.assertIsNone(self.gen._pick_most_recent([p], last_found=True))

    def test_picks_newest_and_extracts_pid(self):
        old = self._touch('core-pid_111.dump', mtime=1000)
        new = self._touch('core-pid_222.dump', mtime=2000)
        result = self.gen._pick_most_recent([old, new], last_found=True)
        self.assertIsNotNone(result)
        path, warning = result
        self.assertEqual(path, new)
        self.assertEqual(self.gen._effective_coredump_pid, '222')
        self.assertIn('high risk', warning)

    def test_warning_kwargs_passed_through(self):
        new = self._touch('core-pid_222.dump', mtime=2000)
        _, warning = self.gen._pick_most_recent([new], name_found=True)
        self.assertIn('matching the crashing program name', warning)


class GenerateCrashLogTest(unittest.TestCase):
    """End-to-end smoke test with the external dependencies stubbed out."""

    def test_no_coredump_emits_help_message_and_sections(self):
        gen = _make_generator(name='WebKitTestRunner', pid=28529)
        gen._get_coredump_path = lambda: (None, None)
        gen._filter_with_cppfilt = lambda text: text
        gen._get_thread_info_for_effective_pid = lambda coredump_found_matches_pid: '(no thread info)\n'

        with mock.patch.object(CrashLogUtils, 'are_coredumps_enabled', return_value=True), \
             mock.patch.object(CrashLogUtils, 'are_coredumps_enabled_and_unlimited', return_value=True):
            stderr, log = gen.generate_crash_log('the test stdout', 'the test stderr')

        # generate_crash_log echoes back the (possibly cppfilt-processed) stderr
        self.assertEqual(stderr, 'the test stderr')

        # Header with process name and pid
        self.assertIn('WebKitTestRunner', log)
        self.assertIn('28529', log)

        # Always-present sections
        for section in ('TEST STDOUT', 'TEST STDERR', 'THREAD NAMING INFO', 'GDB FULL BACKTRACE'):
            self.assertIn(section, log)

        # The supplied stdout/stderr text appears under those sections
        self.assertIn('the test stdout', log)
        self.assertIn('the test stderr', log)

        # The help message replaces the missing backtrace, mentions the pid
        self.assertIn('Coredump for pid 28529 not found', log)

    def test_stderr_section_skipped_when_no_stderr(self):
        gen = _make_generator(name='WebKitWebProcess', pid=42)
        gen._get_coredump_path = lambda: (None, None)
        gen._filter_with_cppfilt = lambda text: text
        gen._get_thread_info_for_effective_pid = lambda coredump_found_matches_pid: '(none)\n'

        with mock.patch.object(CrashLogUtils, 'are_coredumps_enabled', return_value=True), \
             mock.patch.object(CrashLogUtils, 'are_coredumps_enabled_and_unlimited', return_value=True):
            _, log = gen.generate_crash_log('stdout content', '')

        self.assertIn('TEST STDOUT', log)
        self.assertNotIn('TEST STDERR', log)


class CorePatternScanRaceTest(unittest.TestCase):
    """Regression: a coredump present at os.listdir() time can be deleted by
    another worker before this one stats it. The scan must skip it instead of
    raising FileNotFoundError and interrupting the whole test suite."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmpdir, True)

    def _touch(self, name, mtime):
        path = os.path.join(self.tmpdir, name)
        with open(path, 'w') as f:
            f.write('')
        os.utime(path, (mtime, mtime))
        return path

    def test_candidate_deleted_between_listdir_and_stat_does_not_crash(self):
        gen = _make_generator(
            name='WebKitWebProcess', pid=222,
            coredump_pattern='core-%e-pid_%p.dump', coredump_directory=self.tmpdir)
        # newer_than makes the scan call getmtime on every candidate (the line
        # that used to crash). Both files are newer than this.
        gen.newer_than = 500
        survivor = self._touch('core-WebKitWebProcess-pid_222.dump', mtime=2000)
        ghost = self._touch('core-WebKitWebProcess-pid_333.dump', mtime=3000)

        real_getmtime = os.path.getmtime

        def flaky_getmtime(path):
            # Simulate 'ghost' being removed by another worker after listdir().
            if os.path.basename(path) == os.path.basename(ghost):
                raise FileNotFoundError(2, 'No such file or directory', path)
            return real_getmtime(path)

        with mock.patch('os.path.getmtime', side_effect=flaky_getmtime), \
             mock.patch('time.sleep'):
            path, warning = gen._get_coredump_path_with_core_pattern_method()

        self.assertEqual(path, survivor)
        self.assertEqual(gen._effective_coredump_pid, 222)


class CoredumpDeletionGatingTest(unittest.TestCase):
    """With auto-delete on, only a pid-matched coredump (and its thread-info
    file) is removed. A name/fallback match is left on disk so the rightful
    owner can still find it; the periodic cleanup reclaims it later."""

    def setUp(self):
        self.coredir = tempfile.mkdtemp()
        self.threaddir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.coredir, True)
        self.addCleanup(shutil.rmtree, self.threaddir, True)

    def _setup(self, pid, effective_pid):
        gen = _make_generator(name='WebKitWebProcess', pid=pid, coredump_directory=self.coredir)
        gen._webkit_thread_info_crashlog_dir = self.threaddir

        core_path = os.path.join(self.coredir, f'core-WebKitWebProcess-pid_{effective_pid}.dump')
        with open(core_path, 'w') as f:
            f.write('ELF-ish coredump bytes')
        info_path = os.path.join(self.threaddir, CrashLogUtils.get_thread_info_file_name(effective_pid))
        with open(info_path, 'w') as f:
            f.write(f'# PID: {effective_pid}\n')

        # Stub selection so the test drives the deletion logic directly, setting
        # the effective pid exactly as a pid match (==) or a name match (!=) would.
        gen._get_coredump_path = lambda: (core_path, None)
        gen._effective_coredump_pid = effective_pid
        # Don't run gdb / cppfilt for real. A non-empty backtrace avoids the help-message path.
        gen._get_gdb_output = lambda path: ('gdb backtrace here\n', '', '')
        gen._filter_with_cppfilt = lambda text: text
        return gen, core_path, info_path

    def test_pid_match_deletes_coredump_and_thread_info(self):
        gen, core_path, info_path = self._setup(pid=222, effective_pid=222)
        with mock.patch.dict(os.environ, {CrashLogEnvVars.core_dumps_autodelete: '1'}):
            gen.generate_crash_log('out', 'err')
        self.assertFalse(os.path.exists(core_path), 'pid-matched coredump should be deleted')
        self.assertFalse(os.path.exists(info_path), 'pid-matched thread-info should be deleted')

    def test_name_match_keeps_coredump_and_thread_info(self):
        # effective pid differs from self.pid => matched by name/fallback, not pid.
        gen, core_path, info_path = self._setup(pid=222, effective_pid=555)
        with mock.patch.dict(os.environ, {CrashLogEnvVars.core_dumps_autodelete: '1'}):
            gen.generate_crash_log('out', 'err')
        self.assertTrue(os.path.exists(core_path), 'non-pid-matched coredump must be left for the owner')
        self.assertTrue(os.path.exists(info_path), 'non-pid-matched thread-info must be left for the owner')

    def test_no_autodelete_keeps_coredump_even_on_pid_match(self):
        gen, core_path, info_path = self._setup(pid=222, effective_pid=222)
        with mock.patch.dict(os.environ, {CrashLogEnvVars.core_dumps_autodelete: '0'}):
            gen.generate_crash_log('out', 'err')
        self.assertTrue(os.path.exists(core_path), 'without auto-delete the raw coredump is kept')

    def test_pid_match_claims_coredump_by_rename_before_gdb(self):
        gen, core_path, info_path = self._setup(pid=222, effective_pid=222)

        # Capture the directory state at the moment gdb runs.
        observed = {}

        def fake_gdb(path):
            observed['gdb_path'] = path
            observed['original_present'] = os.path.exists(core_path)
            observed['claimed_present'] = any(
                n.startswith(CrashLogUtils.CLAIMED_COREDUMP_PREFIX) for n in os.listdir(self.coredir))
            return ('gdb backtrace here\n', '', '')

        gen._get_gdb_output = fake_gdb
        with mock.patch.dict(os.environ, {CrashLogEnvVars.core_dumps_autodelete: '1'}):
            gen.generate_crash_log('out', 'err')

        # During gdb the core had already been renamed out of the way.
        self.assertFalse(observed['original_present'], 'core should be renamed before gdb runs')
        self.assertTrue(observed['claimed_present'], 'a claimed-prefixed file should exist during gdb')
        self.assertTrue(os.path.basename(observed['gdb_path']).startswith(CrashLogUtils.CLAIMED_COREDUMP_PREFIX))
        # Afterwards nothing is left behind (neither the original nor the claimed file).
        self.assertFalse(os.path.exists(core_path))
        self.assertFalse(
            any(n.startswith(CrashLogUtils.CLAIMED_COREDUMP_PREFIX) for n in os.listdir(self.coredir)),
            'claimed file must be removed after processing')

    def test_name_match_is_not_claimed_by_rename(self):
        # A non-pid match is left in place (not renamed), so the rightful owner can find it.
        gen, core_path, info_path = self._setup(pid=222, effective_pid=555)
        observed = {}

        def fake_gdb(path):
            observed['original_present'] = os.path.exists(core_path)
            return ('gdb backtrace here\n', '', '')

        gen._get_gdb_output = fake_gdb
        with mock.patch.dict(os.environ, {CrashLogEnvVars.core_dumps_autodelete: '1'}):
            gen.generate_crash_log('out', 'err')
        self.assertTrue(observed['original_present'], 'name-matched core must not be renamed')
        self.assertTrue(os.path.exists(core_path))

    def test_coredump_gone_at_stat_time_skips_rename_and_gdb(self):
        # The core passed the isfile() check but vanished before we could stat its size:
        # we must not rename it nor spawn gdb on a missing file.
        gen, core_path, info_path = self._setup(pid=222, effective_pid=222)
        gdb_called = {'v': False}

        def fake_gdb(path):
            gdb_called['v'] = True
            return ('should not be reached\n', '', '')

        gen._get_gdb_output = fake_gdb
        with mock.patch.object(CrashLogUtils, 'human_readable_size', side_effect=OSError(2, 'No such file or directory')), \
             mock.patch.dict(os.environ, {CrashLogEnvVars.core_dumps_autodelete: '1'}):
            _, log = gen.generate_crash_log('out', 'err')

        self.assertFalse(gdb_called['v'], 'gdb must not run when the coredump vanished before stat')
        self.assertIn('removed', log)
        self.assertFalse(
            any(n.startswith(CrashLogUtils.CLAIMED_COREDUMP_PREFIX) for n in os.listdir(self.coredir)),
            'a vanished coredump must not be renamed/claimed')


class CleanOldCoredumpsTest(unittest.TestCase):
    """clean_old_coredumps() reaps old files matching the core_pattern AND old
    claimed (being-processed) leftovers, while leaving fresh and unrelated files."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmpdir, True)
        # clean_old_coredumps lives on the startup handler; build a bare instance
        # (its __init__ has heavy side effects) and set only what the method uses.
        self.handler = GDBCrashLogStartupHandler.__new__(GDBCrashLogStartupHandler)
        self.handler._coredump_directory = self.tmpdir
        self.handler._coredump_pattern = 'core-%e-pid_%p.dump'

    def _touch(self, name, age_seconds):
        path = os.path.join(self.tmpdir, name)
        with open(path, 'w') as f:
            f.write('')
        mtime = time.time() - age_seconds
        os.utime(path, (mtime, mtime))
        return path

    def test_reaps_old_pattern_and_claimed_keeps_fresh_and_unrelated(self):
        old_core = self._touch('core-WebKitWebProcess-pid_111.dump', age_seconds=3600)
        fresh_core = self._touch('core-WebKitWebProcess-pid_222.dump', age_seconds=5)
        old_claimed = self._touch(
            CrashLogUtils.CLAIMED_COREDUMP_PREFIX + 'core-WebKitWebProcess-pid_333.dump', age_seconds=3600)
        fresh_claimed = self._touch(
            CrashLogUtils.CLAIMED_COREDUMP_PREFIX + 'core-WebKitWebProcess-pid_444.dump', age_seconds=5)
        unrelated = self._touch('not-a-coredump.txt', age_seconds=3600)

        self.handler.clean_old_coredumps()

        self.assertFalse(os.path.exists(old_core), 'old pattern-matching core should be reaped')
        self.assertTrue(os.path.exists(fresh_core), 'fresh core should be kept')
        self.assertFalse(os.path.exists(old_claimed), 'old claimed leftover should be reaped')
        self.assertTrue(os.path.exists(fresh_claimed), 'fresh (in-flight) claimed file should be kept')
        self.assertTrue(os.path.exists(unrelated), 'unrelated file should be left untouched')


class DetermineCoredumpMethodAndDirTest(unittest.TestCase):

    def _run_with_core_pattern(self, core_pattern, coredumpctl_path=None):
        with mock.patch('builtins.open', mock.mock_open(read_data=core_pattern + '\n')), \
             mock.patch('shutil.which', return_value=coredumpctl_path):
            return CrashLogUtils.determine_coredump_method_and_dir()

    def test_abspath_method_simple(self):
        method, pattern, directory, has_var = self._run_with_core_pattern('/var/tmp/core/core-%e-%p.dump')
        self.assertEqual(method, CoreDumpMethod.Abspath)
        self.assertEqual(pattern, 'core-%e-%p.dump')
        self.assertEqual(directory, '/var/tmp/core')
        self.assertFalse(has_var)

    def test_abspath_with_fixed_specifier_in_dir(self):
        # %u is fixed for the session (the uid) so it expands at config time and the directory is NOT flagged as variable.
        method, pattern, directory, has_var = self._run_with_core_pattern('/var/tmp/core/u%u/core-%p')
        self.assertEqual(method, CoreDumpMethod.Abspath)
        self.assertEqual(directory, f'/var/tmp/core/u{os.getuid()}')
        self.assertFalse(has_var)

    def test_abspath_with_variable_specifier_in_dir(self):
        # %p in the directory changes per crash: flagged as variable.
        method, _, _, has_var = self._run_with_core_pattern('/var/tmp/core/%p/core')
        self.assertEqual(method, CoreDumpMethod.Abspath)
        self.assertTrue(has_var)

    def test_systemd_pipe_with_coredumpctl_present(self):
        method, _, directory, _ = self._run_with_core_pattern('|/usr/lib/systemd/systemd-coredump %P %u', coredumpctl_path='/usr/bin/coredumpctl')
        self.assertEqual(method, CoreDumpMethod.Coredumpctl)
        self.assertEqual(directory, '/var/lib/systemd/coredump')

    def test_systemd_pipe_without_coredumpctl_program(self):
        method, _, _, _ = self._run_with_core_pattern('|/usr/lib/systemd/systemd-coredump %P', coredumpctl_path=None)
        # coredumpctl isn't on PATH: we can't use the systemd path, method is Unknown.
        self.assertEqual(method, CoreDumpMethod.Unknown)

    def test_pipe_to_non_systemd_program(self):
        method, _, _, _ = self._run_with_core_pattern('|/usr/share/apport/apport %P', coredumpctl_path='/usr/bin/coredumpctl')
        # Apport pipe handler, not systemd-coredump: Unknown.
        self.assertEqual(method, CoreDumpMethod.Unknown)

    def test_unknown_when_pattern_is_relative(self):
        # A relative name (no leading '/', '|', or '@') means "in the crashing
        # process's CWD", which we can't reliably use. Treated as Unknown.
        method, _, _, _ = self._run_with_core_pattern('core')
        self.assertEqual(method, CoreDumpMethod.Unknown)


class GetTempDirForCoredumpctlDumpsTest(unittest.TestCase):

    def setUp(self):
        # Clear the @memoized cache so each test case gets fresh probing logic.
        CrashLogUtils.__dict__['get_temp_dir_for_coredumpctl_dumps'].__func__._results_cache.clear()
        # /proc/mounts is opened once per is_tmpfs() call (twice total).
        # Tests override `self.proc_mounts_contents` to mark a path as tmpfs.
        self.proc_mounts_contents = "proc /proc proc rw 0 0\n"
        self.writable = {'/tmp': True, '/var/tmp': True}
        self.free = {'/tmp': 100 * 1024 ** 3, '/var/tmp': 100 * 1024 ** 3}
        self._disk_usage = collections.namedtuple('disk_usage', ['total', 'used', 'free'])

    def _run(self):
        real_open = builtins.open

        def fake_open(path, *args, **kwargs):
            if path == '/proc/mounts':
                return io.StringIO(self.proc_mounts_contents)
            return real_open(path, *args, **kwargs)

        with mock.patch('tempfile.gettempdir', return_value='/tmp'), \
             mock.patch('os.access', side_effect=lambda p, mode: self.writable.get(p, False)), \
             mock.patch('os.path.realpath', side_effect=lambda p: p), \
             mock.patch('builtins.open', side_effect=fake_open), \
             mock.patch('shutil.disk_usage', side_effect=lambda p: self._disk_usage(0, 0, self.free[p])), \
             mock.patch('os.makedirs') as makedirs:
            result = CrashLogUtils.get_temp_dir_for_coredumpctl_dumps()
        return result, makedirs

    def test_picks_tmp_when_only_tmp_writable(self):
        self.writable = {'/tmp': True, '/var/tmp': False}
        result, makedirs = self._run()
        self.assertEqual(result, '/tmp/webkit-coredumpctl-dumps')
        makedirs.assert_called_once_with('/tmp/webkit-coredumpctl-dumps', exist_ok=True)

    def test_picks_var_tmp_when_only_var_tmp_writable(self):
        self.writable = {'/tmp': False, '/var/tmp': True}
        result, _ = self._run()
        self.assertEqual(result, '/var/tmp/webkit-coredumpctl-dumps')

    def test_picks_tmp_when_free_space_is_similar(self):
        # Both writable, identical free space: preserves the order (tmp first).
        result, _ = self._run()
        self.assertEqual(result, '/tmp/webkit-coredumpctl-dumps')

    def test_picks_var_tmp_when_substantially_more_free(self):
        # /var/tmp has +2 GB more than /tmp: prefer /var/tmp.
        self.free = {'/tmp': 1 * 1024 ** 3, '/var/tmp': 3 * 1024 ** 3}
        result, _ = self._run()
        self.assertEqual(result, '/var/tmp/webkit-coredumpctl-dumps')

    def test_skips_tmpfs_mount(self):
        # /tmp is tmpfs (bad for big coredumps): /var/tmp wins.
        self.proc_mounts_contents = "tmpfs /tmp tmpfs rw 0 0\n"
        result, _ = self._run()
        self.assertEqual(result, '/var/tmp/webkit-coredumpctl-dumps')

    def test_falls_back_to_tmp_when_nothing_usable(self):
        # Neither writable: function warns and still returns a tmp subdir.
        self.writable = {'/tmp': False, '/var/tmp': False}
        result, _ = self._run()
        self.assertEqual(result, '/tmp/webkit-coredumpctl-dumps')


class ThreadNamesCrashLogCapturerTest(unittest.TestCase):

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmpdir, True)
        self.capturer = ThreadNamesCrashLogCapturer.__new__(ThreadNamesCrashLogCapturer)
        self.capturer._coredump_method = CoreDumpMethod.Abspath
        self.capturer._webkit_thread_info_crashlog_dir = self.tmpdir

    def _read_output(self, pid):
        path = os.path.join(self.tmpdir, CrashLogUtils.get_thread_info_file_name(pid))
        self.assertTrue(os.path.isfile(path), f'expected output file at {path}')
        with open(path) as f:
            return f.read()

    def test_capturer_is_disabled_when_pattern_only_contains_literal_percent_p(self):
        with mock.patch.object(CrashLogUtils, 'get_thread_data_dir', return_value=self.tmpdir), \
             mock.patch.object(CrashLogUtils, 'determine_coredump_method_and_dir', return_value=(CoreDumpMethod.Abspath, 'core-%%p.dump', self.tmpdir, False)), \
             mock.patch('os.access', return_value=True), \
             mock.patch('threading.Thread') as thread_constructor:
            ThreadNamesCrashLogCapturer()
        thread_constructor.assert_not_called()

    def test_repeated_pid_specifiers_reuse_the_same_pid_capture(self):
        regex = CrashLogUtils.core_pattern_to_regex('core-%p-again-%p.dump', generate_regex_for_pid_match=True,)

        match = re.fullmatch(regex, 'core-1234-again-1234.dump')
        self.assertIsNotNone(match)
        self.assertEqual(match.group('pid'), '1234')

        # Both %p expansions refer to the same crashing PID, so a different
        # second PID should not match.
        self.assertIsNone(re.fullmatch(regex, 'core-1234-again-5678.dump'))

    def test_writes_thread_table_when_threads_present(self):
        threads = {'100': 'main', '101': 'gc-worker', '102': 'io-thread'}
        with mock.patch.object(self.capturer, 'read_thread_info', return_value=threads):
            self.capturer.handle_coredump('9999')

        content = self._read_output('9999')
        self.assertIn('# Thread info captured at', content)
        self.assertIn('# Number of threads: 3', content)
        self.assertIn('# PID: 9999', content)
        for tid, name in threads.items():
            self.assertIn(tid, content)
            self.assertIn(name, content)
        # TIDs should be sorted numerically. Each thread row starts on its own line.
        idx_100 = content.find('\n100')
        idx_101 = content.find('\n101')
        idx_102 = content.find('\n102')
        self.assertTrue(0 < idx_100 < idx_101 < idx_102)

    def test_writes_error_when_process_already_gone(self):
        with mock.patch.object(self.capturer, 'read_thread_info', return_value=None):
            self.capturer.handle_coredump('1234')

        content = self._read_output('1234')
        self.assertIn('ERROR', content)
        self.assertIn('1234 was already gone', content)
        # Headers/table should NOT appear in the error case
        self.assertNotIn('# Number of threads', content)
        self.assertNotIn('# PID:', content)
        self.assertNotIn('TID', content)

    def test_writes_warning_when_no_threads_found(self):
        with mock.patch.object(self.capturer, 'read_thread_info', return_value={}):
            self.capturer.handle_coredump('1234')

        content = self._read_output('1234')
        self.assertIn('WARNING', content)
        self.assertIn('No threads found', content)
        self.assertNotIn('# Number of threads', content)

    def test_full_path_through_fake_proc_task(self):
        # Drive handle_coredump through the real read_thread_info, with the
        # /proc/<pid>/task lookups faked at the os.listdir / open layer.
        fake_pid = '9999'
        fake_tids = ['100', '101', '102']
        fake_comms = {'100': 'main\n', '101': 'gc-worker\n', '102': 'io-thread\n'}
        task_dir = f'/proc/{fake_pid}/task'

        real_listdir = os.listdir
        real_open = builtins.open

        def fake_listdir(path):
            if path == task_dir:
                return list(fake_tids)
            return real_listdir(path)

        def fake_open(path, *args, **kwargs):
            for tid, name in fake_comms.items():
                if path == f'{task_dir}/{tid}/comm':
                    return io.StringIO(name)
            return real_open(path, *args, **kwargs)

        with mock.patch('os.listdir', side_effect=fake_listdir), \
             mock.patch('builtins.open', side_effect=fake_open):
            self.capturer.handle_coredump(fake_pid)

        content = self._read_output(fake_pid)
        self.assertIn('# Number of threads: 3', content)
        self.assertIn(f'# PID: {fake_pid}', content)
        for name in fake_comms.values():
            self.assertIn(name.strip(), content)


# Build a directory of fake coredump files with controlled mtimes and check that the selection logic
# returns the right file for various queries (pid match, name fallback, newer_than filtering).
# Uses a real tempdir so os.listdir / os.path.getmtime work without patching.
class GetCoredumpPathTest(unittest.TestCase):

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmpdir, True)
        self.gen = _make_generator(
            name='WebKitWebProcess',
            pid=222,
            coredump_pattern='core-%e-pid_%p.dump',
            coredump_directory=self.tmpdir,
        )

    def _touch(self, name, mtime):
        path = os.path.join(self.tmpdir, name)
        with open(path, 'w') as f:
            f.write('')
        os.utime(path, (mtime, mtime))
        return path

    def test_matches_by_pid_when_available(self):
        # Multiple coredumps in the dir, one matching our pid.
        self._touch('core-OtherProcess-pid_111.dump', mtime=1000)
        expected = self._touch('core-WebKitWebProcess-pid_222.dump', mtime=2000)
        self._touch('core-WebKitWebProcess-pid_333.dump', mtime=3000)  # newer but wrong pid

        # Patch out the retry sleep so the test runs fast.
        with mock.patch('time.sleep'):
            path, warning = self.gen._get_coredump_path_with_core_pattern_method()

        self.assertEqual(path, expected)
        self.assertEqual(self.gen._effective_coredump_pid, 222)
        # pid match: no warning
        self.assertIsNone(warning)

    def test_selection_ignores_claimed_prefixed_files(self):
        # A coredump another worker is processing (renamed with the claimed prefix) must be
        # invisible to this scan, even though without the prefix it would win by pid+mtime.
        claimed = self._touch(
            CrashLogUtils.CLAIMED_COREDUMP_PREFIX + 'core-WebKitWebProcess-pid_222.dump', mtime=3000)
        expected = self._touch('core-WebKitWebProcess-pid_222.dump', mtime=2000)

        with mock.patch('time.sleep'):
            path, warning = self.gen._get_coredump_path_with_core_pattern_method()

        self.assertEqual(path, expected)
        self.assertNotEqual(path, claimed)

    def test_matches_by_pid_when_pattern_contains_literal_percent_p(self):
        gen = _make_generator(name='WebKitWebProcess', pid=222, coredump_pattern='core-%%p-pid_%p.dump', coredump_directory=self.tmpdir)

        expected = self._touch('core-%p-pid_222.dump', mtime=2000)
        self._touch('core-%p-pid_111.dump', mtime=3000)

        with mock.patch('time.sleep'):
            path, warning = gen._get_coredump_path_with_core_pattern_method()

        self.assertEqual(path, expected)
        self.assertEqual(gen._effective_coredump_pid, 222)
        self.assertIsNone(warning)

    def test_does_not_fall_back_to_name_when_pid_match_is_possible_and_fallback_is_disabled(self):
        # pid 222 is valid and the pattern contains %p, so matching by pid is possible.
        # There are matching program-name coredumps, but none for pid 222.
        self._touch('core-WebKitWebProcess-pid_111.dump', mtime=1000)
        self._touch('core-WebKitWebProcess-pid_333.dump', mtime=2000)

        with mock.patch('time.sleep') as sleep, \
             mock.patch.object(CrashLogUtils, 'allow_unreliable_fallback_to_latest_coredump', return_value=False):
            path, warning = self.gen._get_coredump_path_with_core_pattern_method()

        self.assertIsNone(path)
        self.assertIn(CrashLogEnvVars.allow_unreliable_fallback_to_latest_coredump, warning)
        sleep.assert_called_once_with(1)

    def test_falls_back_to_name_when_pid_match_is_possible_and_fallback_is_enabled(self):
        # pid 222 is valid and the pattern contains %p, so matching by pid is possible.
        # There is no pid match, but unreliable fallback is explicitly enabled.
        self._touch('core-OtherProcess-pid_111.dump', mtime=1000)
        expected = self._touch('core-WebKitWebProcess-pid_555.dump', mtime=2000)

        with mock.patch('time.sleep') as sleep, \
             mock.patch.object(CrashLogUtils, 'allow_unreliable_fallback_to_latest_coredump', return_value=True):
            path, warning = self.gen._get_coredump_path_with_core_pattern_method()

        self.assertEqual(path, expected)
        self.assertEqual(self.gen._effective_coredump_pid, '555')
        self.assertIn('matching the crashing program name', warning)
        sleep.assert_called_once_with(1)

    def test_falls_back_to_name_when_pid_is_invalid_and_fallback_is_disabled(self):
        # With no valid pid, matching by pid is impossible, so name-based matching
        # is allowed even without the unreliable-fallback env var.
        self.gen.pid = None

        self._touch('core-OtherProcess-pid_111.dump', mtime=1000)
        expected = self._touch('core-WebKitWebProcess-pid_555.dump', mtime=2000)

        with mock.patch('time.sleep') as sleep, \
             mock.patch.object(CrashLogUtils, 'allow_unreliable_fallback_to_latest_coredump', return_value=False):
            path, warning = self.gen._get_coredump_path_with_core_pattern_method()

        self.assertEqual(path, expected)
        self.assertEqual(self.gen._effective_coredump_pid, '555')
        self.assertIn('matching the crashing program name', warning)
        sleep.assert_not_called()

    def test_falls_back_to_name_when_core_pattern_cannot_match_pid_and_fallback_is_disabled(self):
        # pid 222 is valid, but this core_pattern does not contain %p, so matching
        # by pid is impossible. Name-based matching is allowed without the env var.
        gen = _make_generator(
            name='WebKitWebProcess',
            pid=222,
            coredump_pattern='core-%e.dump',
            coredump_directory=self.tmpdir,
        )

        self._touch('core-OtherProcess.dump', mtime=1000)
        expected = self._touch('core-WebKitWebProcess.dump', mtime=2000)

        with mock.patch('time.sleep') as sleep, \
             mock.patch.object(CrashLogUtils, 'allow_unreliable_fallback_to_latest_coredump', return_value=False):
            path, warning = gen._get_coredump_path_with_core_pattern_method()

        self.assertEqual(path, expected)
        self.assertIsNone(gen._effective_coredump_pid)
        self.assertIn('matching the crashing program name', warning)
        sleep.assert_not_called()

    def test_pid_match_on_second_attempt_wins_over_existing_name_match(self):
        # If pid matching is possible, the function waits one retry before taking
        # the unreliable name fallback. If the exact pid match appears during that
        # window, it must win.
        name_match = self._touch('core-WebKitWebProcess-pid_333.dump', mtime=1000)
        expected = os.path.join(self.tmpdir, 'core-WebKitWebProcess-pid_222.dump')

        def create_pid_match(_seconds):
            with open(expected, 'w') as f:
                f.write('')
            os.utime(expected, (2000, 2000))

        with mock.patch('time.sleep', side_effect=create_pid_match) as sleep, \
             mock.patch.object(CrashLogUtils, 'allow_unreliable_fallback_to_latest_coredump', return_value=True):
            path, warning = self.gen._get_coredump_path_with_core_pattern_method()

        self.assertEqual(path, expected)
        self.assertNotEqual(path, name_match)
        self.assertEqual(self.gen._effective_coredump_pid, 222)
        self.assertIsNone(warning)
        sleep.assert_called_once_with(1)

    def test_pattern_with_literal_percent_p_has_no_pid_specifier(self):
        self.gen._coredump_pattern = 'core-%%p.dump'
        self.assertFalse(self.gen._coredump_pattern_has_pid_format_string())

    def test_pid_lookup_ignores_filename_with_extra_suffix(self):
        matching_core = self._touch('core-WebKitWebProcess-pid_12345.dump', mtime=1000, )
        self._touch('core-WebKitWebProcess-pid_12345.dump.bak', mtime=2000, )

        gen = _make_generator(name='WebKitWebProcess', pid=12345, coredump_pattern='core-%f-pid_%p.dump', coredump_directory=self.tmpdir)
        with mock.patch('time.sleep'):
            coredump_path, warning = gen._get_coredump_path_with_core_pattern_method()

        self.assertEqual(coredump_path, matching_core)
        self.assertIsNone(warning)
        self.assertEqual(gen._effective_coredump_pid, 12345)

    def test_returns_none_without_unreliable_fallback(self):
        # No pid match and fallback disabled: no path, warning recommending the env var.
        self._touch('core-OtherProcess-pid_111.dump', mtime=1000)

        with mock.patch('time.sleep'), \
             mock.patch.object(CrashLogUtils, 'allow_unreliable_fallback_to_latest_coredump', return_value=False):
            path, warning = self.gen._get_coredump_path_with_core_pattern_method()

        self.assertIsNone(path)
        self.assertIn(CrashLogEnvVars.allow_unreliable_fallback_to_latest_coredump, warning)

    def test_filters_by_newer_than(self):
        # Older-than-cutoff coredump for the pid should be excluded.
        self._touch('core-WebKitWebProcess-pid_222.dump', mtime=1000)
        self.gen.newer_than = 1500

        with mock.patch('time.sleep'), \
             mock.patch.object(CrashLogUtils, 'allow_unreliable_fallback_to_latest_coredump',
                               return_value=False):
            path, _ = self.gen._get_coredump_path_with_core_pattern_method()

        self.assertIsNone(path)


class GetCoredumpPathWithCoredumpctlTest(unittest.TestCase):

    def setUp(self):
        self.gen = _make_generator(name='WebKitWebProcess', pid=222, coredump_method=CoreDumpMethod.Coredumpctl)

    def _entries_json(self, *entries):
        return json.dumps(list(entries))

    def _entry(self, pid, exe, time='2026-05-19T12:00:00+0000'):
        return {'pid': pid, 'exe': exe, 'time': time}

    def _dump_core_side_effect(self, args, warning):
        return '/tmp/dumped-core', warning

    def test_matches_by_pid_when_pid_is_valid_and_in_host_namespace(self):
        entries = self._entries_json(
            self._entry(111, '/usr/libexec/WebKitWebProcess'),
            self._entry(222, '/usr/libexec/WebKitWebProcess'),
        )

        with mock.patch.object(CrashLogUtils, 'in_host_pid_namespace', return_value=True), \
             mock.patch.object(CrashLogUtils, 'allow_unreliable_fallback_to_latest_coredump', return_value=False), \
             mock.patch.object(self.gen._executive, 'run_command', return_value=entries), \
             mock.patch.object(self.gen, '_coredumpctl_dump_core_for_pid', side_effect=self._dump_core_side_effect) as dump_core:
            path, warning = self.gen._get_coredump_path_with_coredumpctl_method()

        self.assertEqual(path, '/tmp/dumped-core')
        self.assertIsNone(warning)
        self.assertEqual(self.gen._effective_coredump_pid, 222)
        dump_core.assert_called_once_with(['222'], None)

    def test_does_not_fall_back_to_name_when_pid_match_is_possible_and_fallback_is_disabled(self):
        # Same executable name exists, but no entry for pid 222.
        entries = self._entries_json(
            self._entry(111, '/usr/libexec/WebKitWebProcess'),
            self._entry(333, '/usr/libexec/WebKitWebProcess'),
        )

        with mock.patch.object(CrashLogUtils, 'in_host_pid_namespace', return_value=True), \
             mock.patch.object(CrashLogUtils, 'allow_unreliable_fallback_to_latest_coredump', return_value=False), \
             mock.patch.object(self.gen._executive, 'run_command', return_value=entries) as run_command, \
             mock.patch.object(self.gen, '_coredumpctl_dump_core_for_pid') as dump_core, \
             mock.patch('time.sleep'):
            path, warning = self.gen._get_coredump_path_with_coredumpctl_method()

        self.assertIsNone(path)
        self.assertIn(CrashLogEnvVars.allow_unreliable_fallback_to_latest_coredump, warning)
        dump_core.assert_not_called()
        self.assertEqual(run_command.call_count, 5)

    def test_falls_back_to_name_when_pid_match_is_possible_but_fallback_is_enabled(self):
        entries = self._entries_json(
            self._entry(111, '/usr/libexec/WebKitWebProcess', time='2026-05-19T12:00:00+0000'),
            self._entry(333, '/usr/libexec/WebKitWebProcess', time='2026-05-19T12:01:00+0000'),
        )

        with mock.patch.object(CrashLogUtils, 'in_host_pid_namespace', return_value=True), \
             mock.patch.object(CrashLogUtils, 'allow_unreliable_fallback_to_latest_coredump', return_value=True), \
             mock.patch.object(self.gen._executive, 'run_command', return_value=entries) as run_command, \
             mock.patch.object(self.gen, '_coredumpctl_dump_core_for_pid', side_effect=self._dump_core_side_effect) as dump_core, \
             mock.patch('time.sleep'):
            path, warning = self.gen._get_coredump_path_with_coredumpctl_method()

        self.assertEqual(path, '/tmp/dumped-core')
        self.assertIn('matching the crashing program name', warning)
        self.assertEqual(self.gen._effective_coredump_pid, 333)
        dump_core.assert_called_once_with(['333'], warning)
        self.assertEqual(run_command.call_count, 5)

    def test_falls_back_to_name_without_env_var_when_running_in_pid_namespace(self):
        entries = self._entries_json(
            self._entry(555, '/usr/libexec/WebKitWebProcess'),
        )

        with mock.patch.object(CrashLogUtils, 'in_host_pid_namespace', return_value=False), \
             mock.patch.object(CrashLogUtils, 'allow_unreliable_fallback_to_latest_coredump', return_value=False), \
             mock.patch.object(self.gen._executive, 'run_command', return_value=entries) as run_command, \
             mock.patch.object(self.gen, '_coredumpctl_dump_core_for_pid', side_effect=self._dump_core_side_effect) as dump_core:
            path, warning = self.gen._get_coredump_path_with_coredumpctl_method()

        self.assertEqual(path, '/tmp/dumped-core')
        self.assertIn('matching the crashing program name', warning)
        self.assertEqual(self.gen._effective_coredump_pid, 555)
        dump_core.assert_called_once_with(['555'], warning)
        self.assertEqual(run_command.call_count, 1)

    def test_falls_back_to_name_without_env_var_when_pid_is_invalid(self):
        self.gen.pid = None

        entries = self._entries_json(
            self._entry(555, '/usr/libexec/WebKitWebProcess'),
        )

        with mock.patch.object(CrashLogUtils, 'in_host_pid_namespace', return_value=True), \
             mock.patch.object(CrashLogUtils, 'allow_unreliable_fallback_to_latest_coredump', return_value=False), \
             mock.patch.object(self.gen._executive, 'run_command', return_value=entries) as run_command, \
             mock.patch.object(self.gen, '_coredumpctl_dump_core_for_pid', side_effect=self._dump_core_side_effect) as dump_core:
            path, warning = self.gen._get_coredump_path_with_coredumpctl_method()

        self.assertEqual(path, '/tmp/dumped-core')
        self.assertIn('matching the crashing program name', warning)
        self.assertEqual(self.gen._effective_coredump_pid, 555)
        dump_core.assert_called_once_with(['555'], warning)
        self.assertEqual(run_command.call_count, 1)

    def test_falls_back_to_latest_only_when_unreliable_fallback_is_enabled(self):
        entries = self._entries_json(
            self._entry(111, '/usr/libexec/OtherProcess', time='2026-05-19T12:00:00+0000'),
            self._entry(999, '/usr/libexec/CompletelyDifferentProcess', time='2026-05-19T12:01:00+0000'),
        )

        with mock.patch.object(CrashLogUtils, 'in_host_pid_namespace', return_value=True), \
             mock.patch.object(CrashLogUtils, 'allow_unreliable_fallback_to_latest_coredump', return_value=True), \
             mock.patch.object(self.gen._executive, 'run_command', return_value=entries), \
             mock.patch.object(self.gen, '_coredumpctl_dump_core_for_pid', side_effect=self._dump_core_side_effect) as dump_core, \
             mock.patch('time.sleep'):
            path, warning = self.gen._get_coredump_path_with_coredumpctl_method()

        self.assertEqual(path, '/tmp/dumped-core')
        self.assertIn('high risk', warning)
        self.assertEqual(self.gen._effective_coredump_pid, 999)
        dump_core.assert_called_once_with(['999'], warning)


class GenerateSimplifiedBacktraceTest(unittest.TestCase):

    _GDB_SAMPLE_ASCENDING = """\
[New LWP 1888937]
[New LWP 1888938]
[Thread debugging using libthread_db enabled]
Core was generated by `/path/to/WebKitTestRunner --args'.
Program terminated with signal SIGSEGV, Segmentation fault.
#0  0x00007f811b32621f in WTFCrash ()
[Current thread is 1 (Thread 0x7f8108f4aac0 (LWP 1888937))]

Thread 1 (Thread 0x7f8108f4aac0 (LWP 1888937)):
#0  0x00007f811b32621f in WTFCrash ()
No symbol table info available.
#1  0x000055b750961cf8 in WTR::UIScriptController::notImplemented() const ()
No symbol table info available.
#2  0x000055b75096212d in WTR::UIScriptController::singleTapAtPoint() ()
No symbol table info available.

Thread 2 (Thread 0x7f8100dff6c0 (LWP 1888938)):
#0  0x00007f8112408d71 in __futex_abstimed_wait_common64 ()
No symbol table info available.
#1  0x00007f811240bc8e in __pthread_cond_wait_common ()
No symbol table info available.
"""

    _GDB_SAMPLE_DESCENDING = """\
[New LWP 1888937]
[New LWP 1888938]
[Thread debugging using libthread_db enabled]
Core was generated by `/path/to/WebKitTestRunner --args'.
Program terminated with signal SIGSEGV, Segmentation fault.
#0  0x00007f811b32621f in WTFCrash ()
[Current thread is 1 (Thread 0x7f8108f4aac0 (LWP 1888937))]

Thread 2 (Thread 0x7f8100dff6c0 (LWP 1888938)):
#0  0x00007f8112408d71 in __futex_abstimed_wait_common64 ()
No symbol table info available.
#1  0x00007f811240bc8e in __pthread_cond_wait_common ()
No symbol table info available.

Thread 1 (Thread 0x7f8108f4aac0 (LWP 1888937)):
#0  0x00007f811b32621f in WTFCrash ()
No symbol table info available.
#1  0x000055b750961cf8 in WTR::UIScriptController::notImplemented() const ()
No symbol table info available.
#2  0x000055b75096212d in WTR::UIScriptController::singleTapAtPoint() ()
No symbol table info available.
"""

    _THREAD_INFO_MAIN_CRASH = """\
# Thread info captured at 2026-05-20T15:30:00
# Number of threads: 2
# PID: 1888937

TID          Thread Name
------------ --------------------
1888937      WebKitTestRunne
1888938      WebKitTestRunne
"""

    def setUp(self):
        self.gen = _make_generator(name='WebKitTestRunner', pid=1888937)

    def _assert_main_thread_crash_output(self, result):
        self.assertIn("Core was generated by `/path/to/WebKitTestRunner --args'.", result)
        self.assertIn("Program terminated with signal SIGSEGV", result)
        self.assertIn(
            "Crashed thread: WebKitTestRunne (PID=1888937, LWP/TID=1888937) (main thread)",
            result)
        self.assertIn("Thread 1 (Thread 0x7f8108f4aac0 (LWP 1888937)):", result)
        # Thread 1 frames are included.
        self.assertIn("WTFCrash ()", result)
        self.assertIn("WTR::UIScriptController::singleTapAtPoint() ()", result)
        # Thread 2 frames are NOT included.
        self.assertNotIn("__futex_abstimed_wait_common64", result)
        self.assertNotIn("__pthread_cond_wait_common", result)
        # Local-variable noise from "bt full" is not included.
        self.assertNotIn("No symbol table info available.", result)

    def test_main_thread_crash_ascending_order(self):
        result = self.gen._generate_simplified_backtrace(
            self._GDB_SAMPLE_ASCENDING, self._THREAD_INFO_MAIN_CRASH)
        self._assert_main_thread_crash_output(result)

    # The default is now to generate the backtrace with "-ascending", but
    # historically it was not, so the parser should support both cases.
    def test_main_thread_crash_descending_order(self):
        result = self.gen._generate_simplified_backtrace(
            self._GDB_SAMPLE_DESCENDING, self._THREAD_INFO_MAIN_CRASH)
        self._assert_main_thread_crash_output(result)

    def test_worker_thread_crash_is_marked_as_not_main(self):
        gdb_backtrace = """\
Core was generated by `/path/to/prog'.
Program terminated with signal SIGSEGV, Segmentation fault.

Thread 1 (Thread 0x7f8108f4aac0 (LWP 12346)):
#0  0x00007f811b32621f in worker_function ()

Thread 2 (Thread 0x7f8100dff6c0 (LWP 12345)):
#0  0x00007f822a326810 in main_thread_idle ()
"""
        thread_info = """\
# PID: 12345

TID          Thread Name
------------ --------------------
12345        MainThread
12346        WorkerThread
"""
        result = self.gen._generate_simplified_backtrace(gdb_backtrace, thread_info)
        self.assertIn(
            "Crashed thread: WorkerThread (PID=12345, LWP/TID=12346) (not main thread)",
            result)
        self.assertIn("worker_function", result)
        self.assertNotIn("main_thread_idle", result)

    def test_thread_name_with_spaces_is_preserved(self):
        gdb_backtrace = """\
Core was generated by `/path/to/prog'.

Thread 1 (Thread 0x7f8108f4aac0 (LWP 99999)):
#0  0x00007f811b32621f in some_function ()

Thread 2 (Thread 0x... (LWP 99998)):
#0  0xAAAA in unrelated ()
"""
        thread_info = """\
# PID: 99999

TID          Thread Name
------------ --------------------
99998        helper
99999        audio sink
"""
        result = self.gen._generate_simplified_backtrace(gdb_backtrace, thread_info)
        self.assertIn("Crashed thread: audio sink (PID=99999, LWP/TID=99999) (main thread)", result)

    def test_thread_10_is_not_treated_as_thread_1(self):
        # `Thread 10` should not match a naive `startswith("Thread 1")` check.
        gdb_backtrace = """\
Core was generated by `/path/to/prog'.

Thread 1 (Thread 0x... (LWP 1001)):
#0  0xBBBB in real_crashing_function ()

Thread 10 (Thread 0x... (LWP 1010)):
#0  0xAAAA in thread_10_function ()
"""
        result = self.gen._generate_simplified_backtrace(gdb_backtrace, "")
        self.assertIn("real_crashing_function", result)
        self.assertNotIn("thread_10_function", result)

    def test_no_thread_info_data(self):
        gdb_backtrace = """\
Core was generated by `/path/to/prog'.
Program terminated with signal SIGSEGV.

Thread 1 (Thread 0x7f8108f4aac0 (LWP 1234)):
#0  0x00007f811b32621f in crash_function ()

Thread 2 (Thread 0x... (LWP 5678)):
#0  0xCAFE in idle_thread ()
"""
        result = self.gen._generate_simplified_backtrace(gdb_backtrace, "")
        # No thread name available: bare header with no PID/TID/main-thread tags.
        self.assertIn("Crashed thread:", result)
        self.assertNotIn("main thread", result)
        self.assertNotIn("PID=", result)
        self.assertIn("crash_function ()", result)
        self.assertNotIn("idle_thread", result)

    def test_thread_info_data_without_matching_tid(self):
        # Thread info file is present but doesn't contain the LWP from the GDB output.
        gdb_backtrace = """\
Core was generated by `/path/to/prog'.

Thread 1 (Thread 0x... (LWP 9999)):
#0  0xFEED in foo ()

Thread 2 (Thread 0x... (LWP 1111)):
#0  0xCAFE in known_thread ()
"""
        thread_info = """\
# PID: 1111

TID          Thread Name
------------ --------------------
1111         MainThread
"""
        result = self.gen._generate_simplified_backtrace(gdb_backtrace, thread_info)
        self.assertIn("Crashed thread:", result)
        # No name found for LWP 9999, so no main/not-main tag is emitted.
        self.assertNotIn("main thread", result)
        self.assertIn("foo ()", result)
        self.assertNotIn("known_thread", result)

    def test_thread_1_line_without_lwp_falls_back_to_bare_header(self):
        # If for any reason LWP info is not there, it should handle it fine.
        gdb_backtrace = """\
Core was generated by `/path/to/prog'.

Thread 1 (Thread 0x7f8108f4aac0):
#0  0xCAFE in foo ()

Thread 2 (Thread 0x... (LWP 5678)):
#0  0xFEED in other ()
"""
        thread_info = """\
# PID: 1234

TID          Thread Name
------------ --------------------
1234         WebKitTestRunne
"""
        result = self.gen._generate_simplified_backtrace(gdb_backtrace, thread_info)
        self.assertIn("Crashed thread:", result)
        self.assertNotIn("WebKitTestRunne", result)
        self.assertNotIn("main thread", result)
        self.assertIn("foo ()", result)
        self.assertNotIn("other", result)

    def test_returns_none_when_no_relevant_lines_present(self):
        result = self.gen._generate_simplified_backtrace("Some unrelated GDB output\nwithout any expected markers\n", "")
        self.assertIsNone(result)

    def test_preamble_frame_before_thread_1_block_is_ignored(self):
        # GDB emits a #0 line at the top describing the current frame, before
        # any "Thread " block. That frame must not be picked.
        gdb_backtrace = """\
Core was generated by `/path/to/prog'.
Program terminated with signal SIGSEGV, Segmentation fault.
#0  0x00007f811b32621f in PREAMBLE_FRAME_should_be_ignored ()
[Current thread is 1 (Thread 0x... (LWP 1234))]

Thread 1 (Thread 0x... (LWP 1234)):
#0  0x12345 in real_thread_1_frame ()

Thread 2 (Thread 0x... (LWP 5678)):
#0  0xCAFE in NON_CRASHING_FRAME_should_be_ignored ()
"""
        result = self.gen._generate_simplified_backtrace(gdb_backtrace, "")
        self.assertIn("real_thread_1_frame ()", result)
        self.assertNotIn("PREAMBLE_FRAME_should_be_ignored", result)
        self.assertNotIn("NON_CRASHING_FRAME_should_be_ignored", result)

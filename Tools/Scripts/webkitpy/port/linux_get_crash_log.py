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

import datetime
import fcntl
import json
import logging
import os
import re
import resource
import requests
import shutil
import subprocess
import sys
import tempfile
import threading
import time

try:
    import inotify_simple
except ImportError:
    inotify_simple = None

from contextlib import nullcontext
from enum import Enum

if sys.version_info < (3, 11):
    class StrEnum(str, Enum):
        def __str__(self):
            return self.value
else:
    from enum import StrEnum

from webkitcorepy import string_utils
from webkitpy.common.memoized import memoized
from webkitpy.common.system.executive import ScriptError
from webkitpy.common.webkit_finder import WebKitFinder


class PrefixAdapter(logging.LoggerAdapter):
    def process(self, msg, kwargs):
        return f"[crash-logger] {msg}", kwargs

    def log(self, level, msg, *args, **kwargs):
        # process() doesn't see the level, so prepend the level name here
        super().log(level, f"{logging.getLevelName(level)}: {msg}", *args, **kwargs)


_log = PrefixAdapter(logging.getLogger(__name__), extra={})


class LockFile:
    def __init__(self, path, slots=1, log_wait_each_seconds=0):
        self.path = path
        self._fd = None
        self._locked_path = None
        self._slots = max(1, slots)
        self._log_wait_seconds = max(0, log_wait_each_seconds)

    def acquire(self):
        if self._log_wait_seconds:
            stop_logging_event = threading.Event()
            logging_thread = threading.Thread(target=self._log_waiter, args=(stop_logging_event,), daemon=True)
            logging_thread.start()
        try:
            if self._slots == 1:
                self._acquire_single()
            else:
                self._acquire_multi()
        finally:
            if self._log_wait_seconds:
                stop_logging_event.set()
                logging_thread.join()

    def _try_claim(self, fd, path):
        try:
            # Returns True if fd is still the current file on disk and claims it.
            if os.fstat(fd.fileno()).st_ino == os.stat(path).st_ino:
                fd.write(str(os.getpid()))
                fd.flush()
                self._fd = fd
                self._locked_path = path
                return True
        except FileNotFoundError:
            pass  # File deleted between open() and stat()
        return False

    def _acquire_single(self):
        while True:
            fd = open(self.path, 'w')
            fcntl.flock(fd, fcntl.LOCK_EX)
            if self._try_claim(fd, self.path):
                # Valid lock acquired.
                return
            # Retry because we got the lock on a deleted file.
            fcntl.flock(fd, fcntl.LOCK_UN)
            fd.close()

    def _acquire_multi(self):
        paths = [f"{self.path}.{i}" for i in range(self._slots)]
        while True:
            for path in paths:
                fd = open(path, 'w')
                try:
                    fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                except BlockingIOError:
                    fd.close()
                    continue
                if self._try_claim(fd, path):
                    return
                fcntl.flock(fd, fcntl.LOCK_UN)
                fd.close()
            time.sleep(1)

    def release(self):
        if self._fd:
            os.unlink(self._locked_path)  # Remove file first.
            fcntl.flock(self._fd, fcntl.LOCK_UN)
            self._fd.close()
            self._fd = None
            self._locked_path = None

    def _log_waiter(self, stop_event):
        if self._slots == 1:
            msg = f"waiting for lockfile '{self.path}' to be released..."
        else:
            msg = f"waiting for a free slot on lockfiles '{self.path}.*' ({self._slots} slots all busy)..."
        while not stop_event.wait(self._log_wait_seconds):
            _log.debug(msg)

    def __enter__(self):
        self.acquire()
        return self

    def __exit__(self, *_):
        self.release()


CORE_PATTERN_RECOMMEND_COMMAND = "echo /var/tmp/core-%f-pid_%p.dump | sudo tee /proc/sys/kernel/core_pattern"


class CoreDumpMethod(StrEnum):
    Unknown = 'unknown'
    Abspath = 'abspath'
    Coredumpctl = 'coredumpctl'


# Env vars to allow advanced fine tuning of this crash logger
class CrashLogEnvVars(StrEnum):
    allow_unreliable_fallback_to_latest_coredump = 'WEBKIT_CRASHLOG_ALLOW_UNRELIABLE_FALLBACK_TO_LATEST_COREDUMP'
    core_dumps_autodelete = 'WEBKIT_CORE_DUMPS_AUTODELETE'
    gdb_concurrent_execution_limit = 'WEBKIT_CRASHLOG_GDB_CONCURRENT_EXECUTION_LIMIT'


class CrashLogUtils:

    @staticmethod
    def get_gdb_concurrent_execution_limit():
        # 0 means no limit
        max_concurrent_gdb_processes = os.environ.get(CrashLogEnvVars.gdb_concurrent_execution_limit, '0')
        if max_concurrent_gdb_processes.isnumeric():
            return int(max_concurrent_gdb_processes)
        _log.warning(f"Value passed is not a number: {CrashLogEnvVars.gdb_concurrent_execution_limit}={max_concurrent_gdb_processes}")
        return 0

    @staticmethod
    @memoized
    def _get_gdb_lock_configuration():
        max_concurrent_gdb_processes = CrashLogUtils.get_gdb_concurrent_execution_limit()
        if max_concurrent_gdb_processes:
            for base in ('/run/lock', os.environ.get('XDG_RUNTIME_DIR'), tempfile.gettempdir(), '/tmp', '/var/tmp'):
                if not base:
                    continue
                lock_base_dir = os.path.join(base, f'webkit-crashlog-{os.getuid()}')
                try:
                    os.makedirs(lock_base_dir, exist_ok=True)
                    if os.path.isdir(lock_base_dir) and os.access(lock_base_dir, os.W_OK | os.X_OK):
                        return os.path.join(lock_base_dir, 'gdb-serial-execution.lock'), max_concurrent_gdb_processes
                except OSError:
                    continue
            _log.warning("No writable directory found for the gdb lock. GDB processes will not be serialized.")
        return None

    # Memoize only the lockfile path but not the lockfile object, which should be unique for each caller.
    @staticmethod
    def get_gdb_lock():
        lock_configuration = CrashLogUtils._get_gdb_lock_configuration()
        if lock_configuration:
            lock_path, slots = lock_configuration
            return LockFile(lock_path, slots=slots, log_wait_each_seconds=60)
        return nullcontext()

    @staticmethod
    def get_thread_info_file_name(pid):
        return f'threadinfo-{pid}.txt'

    @staticmethod
    @memoized
    def get_thread_data_dir():
        return os.path.join(tempfile.gettempdir(), f'webkit-crashlog-thread-name-data-{os.getuid()}')

    @staticmethod
    @memoized
    def get_temp_dir_for_coredumpctl_dumps():
        def is_tmpfs(path):
            real_path = os.path.realpath(path)
            with open('/proc/mounts') as f:
                for line in f:
                    fields = line.split()
                    if fields[1] == real_path and fields[2] == 'tmpfs':
                        return True
            return False

        def check_and_return_subdir(tmp_dir):
            tmp_dir = os.path.join(tmp_dir, 'webkit-coredumpctl-dumps')
            os.makedirs(tmp_dir, exist_ok=True)
            _log.debug(f"Temporal directory for dumping cores with coredumpctl: {tmp_dir}")
            return tmp_dir

        candidates = []
        for tmp_dir in [tempfile.gettempdir(), '/var/tmp']:
            if os.access(tmp_dir, os.W_OK | os.X_OK) and not is_tmpfs(tmp_dir):
                candidates.append(tmp_dir)

        if not candidates:
            _log.warning("Can't find a writable temporary directory not backed by tmpfs. This may cause OOM issues when dumping large coredumps from Debug builds.")
            return check_and_return_subdir(tempfile.gettempdir())

        if len(candidates) == 1:
            return check_and_return_subdir(candidates[0])

        # Pick the second option (/var/tmp) if it has significantly more free space (+1GB diff)
        if shutil.disk_usage(candidates[1]).free - shutil.disk_usage(candidates[0]).free > 1024**3:
            return check_and_return_subdir(candidates[1])
        return check_and_return_subdir(candidates[0])

    @staticmethod
    def allow_unreliable_fallback_to_latest_coredump():
        # If this is enabled then instead of giving up when it can't find the coredump by the pid,
        # it will pick the last one available, and that is unreliable with several parallel workers.
        return os.environ.get(CrashLogEnvVars.allow_unreliable_fallback_to_latest_coredump, '0') == '1'

    @staticmethod
    def expand_core_pattern_and_detect_variable_specifiers(core_pattern):
        # Expand the format specifiers that don't change between different
        # crashes of the same user session and detect if there are any other
        # variable format specifiers in the string after resolving those.
        rlimit_core = resource.getrlimit(resource.RLIMIT_CORE)[0]
        if rlimit_core == resource.RLIM_INFINITY:
            rlimit_core = 2**64 - 1

        fixed_format_specifiers = {
            '%u': str(os.getuid()),
            '%g': str(os.getgid()),
            '%h': os.uname().nodename,
            '%c': str(rlimit_core),
        }

        core_pattern = core_pattern.replace('%%', '\x00')
        for format_specifier, value in fixed_format_specifiers.items():
            core_pattern = core_pattern.replace(format_specifier, value)
        core_pattern = core_pattern.rstrip('%')

        # Check before restoring literal %
        has_variable_format_specifiers = '%' in core_pattern
        core_pattern = core_pattern.replace('\x00', '%')

        return core_pattern, has_variable_format_specifiers

    @staticmethod
    def determine_coredump_method_and_dir():
        coredump_method = CoreDumpMethod.Unknown
        coredump_pattern = None
        coredump_directory = None
        coredump_directory_has_variable_format_specifiers = False
        with open("/proc/sys/kernel/core_pattern", "r") as f:
            core_pattern = f.read().strip()
        if core_pattern.startswith("/"):
            coredump_directory = os.path.dirname(core_pattern)
            if '%' in coredump_directory:
                coredump_directory, coredump_directory_has_variable_format_specifiers = CrashLogUtils.expand_core_pattern_and_detect_variable_specifiers(coredump_directory)
            coredump_pattern = os.path.basename(core_pattern)
            coredump_method = CoreDumpMethod.Abspath
            # When coredump_pattern doesn't include %p but core_uses_pid is 1 then the kernel appends .%p
            if '%p' not in core_pattern.replace('%%', ''):
                with open("/proc/sys/kernel/core_uses_pid", "r") as f:
                    core_uses_pid = f.read().strip()
                    if core_uses_pid == "1":
                        coredump_pattern += '.%p'
        elif any(core_pattern.startswith(s) for s in ("|", "@")):
            # pipe to program or socket
            if 'systemd' in core_pattern and shutil.which('coredumpctl'):
                coredump_method = CoreDumpMethod.Coredumpctl
                coredump_directory = '/var/lib/systemd/coredump'
        return coredump_method, coredump_pattern, coredump_directory, coredump_directory_has_variable_format_specifiers

    @staticmethod
    def core_pattern_to_regex(core_pattern, generate_regex_for_pid_match=False):
        """
        Convert a core_pattern string to a regex, mirroring kernel expansion rules:
          %%        -> literal %
          %<NUL>    -> dropped (% at end of string)
          %<valid>  -> .+   (non-empty, the kernel always writes something)
          %<other>  -> dropped (both % and the char)
          See: https://www.kernel.org/doc/html/latest/admin-guide/sysctl/kernel.html#core-pattern
        """
        valid_specifiers = set("pPiIugdsthecfECF")
        pid_capture_emitted = False
        result = []
        i = 0
        while i < len(core_pattern):
            if core_pattern[i] != "%":
                result.append(re.escape(core_pattern[i]))
                i += 1
                continue

            # we have a '%'
            if i + 1 >= len(core_pattern):
                # %<NUL>: drop the %
                break

            specifier = core_pattern[i + 1]
            if specifier == "%":
                result.append("%")          # %% -> literal %
            elif generate_regex_for_pid_match and specifier == 'p':
                if pid_capture_emitted:
                    # Repeated %p instances represent the same crashing PID.
                    result.append(r'(?P=pid)')
                else:
                    result.append(r'(?P<pid>\d+)')
                    pid_capture_emitted = True
            elif specifier in valid_specifiers:
                result.append(".+")         # valid specifier -> wildcard
            # else: %<other> -> drop both silently

            i += 2

        return "".join(result)

    @staticmethod
    def are_coredumps_enabled(coredump_method):
        # RLIMIT_CORE is ignored when the system is configured to pipe core dumps to a program
        if coredump_method == CoreDumpMethod.Coredumpctl:
            return True
        soft, hard = resource.getrlimit(resource.RLIMIT_CORE)
        return soft != 0

    @staticmethod
    def are_coredumps_enabled_and_unlimited(coredump_method):
        if coredump_method == CoreDumpMethod.Coredumpctl:
            return True
        soft, hard = resource.getrlimit(resource.RLIMIT_CORE)
        return soft == resource.RLIM_INFINITY

    @staticmethod
    def human_readable_size(path):
        def fmt(size):
            for unit in ('B', 'KB', 'MB', 'GB', 'TB'):
                if size < 1024:
                    return f'{size:.1f} {unit}'
                size /= 1024
            return f'{size:.1f} PB'

        st = os.stat(path)
        logical = st.st_size
        actual = st.st_blocks * 512

        if logical == actual:
            return fmt(logical)
        return f'{fmt(logical)} (sparse, {fmt(actual)} on disk)'

    @staticmethod
    def make_temp_path(suffix='', prefix='tmp', dir=None):
        # Safe replacement for the deprecated tempfile.mktemp().
        fd, path = tempfile.mkstemp(suffix=suffix, prefix=prefix, dir=dir)
        os.close(fd)
        return path

    @staticmethod
    @memoized
    def in_host_pid_namespace():
        PROC_PID_INIT_INO = 0xEFFFFFFC  # PROC_PID_INIT_INO from include/linux/proc_ns.h
        try:
            return os.stat("/proc/self/ns/pid").st_ino == PROC_PID_INIT_INO
        except OSError:
            return True  # assume host namespace when we can't tell

    @staticmethod
    def core_pattern_has_pid_format_string(core_pattern):
        pid_regex = CrashLogUtils.core_pattern_to_regex(core_pattern, generate_regex_for_pid_match=True)
        return '(?P<pid>' in pid_regex


# This runs inside a thread that watches (with inotify) the directory where the coredumps are generated
# And saves the names of the threads of the crashing pid as soon as the coredump is started to be generated,
# this info is later added into the generated crash log report. Even when it is naturally racy it works
# pretty well in practice in the case of WebKit coredumps that are big and take time to be generated.
class ThreadNamesCrashLogCapturer(object):

    def __init__(self):
        self._webkit_thread_info_crashlog_dir = CrashLogUtils.get_thread_data_dir()
        try:
            os.makedirs(self._webkit_thread_info_crashlog_dir, exist_ok=True)
        except OSError as e:
            _log.warning(f"Crash log thread name capturer disabled: Failed to create directory {self._webkit_thread_info_crashlog_dir}: {e.strerror}")
            return
        if not os.access(self._webkit_thread_info_crashlog_dir, os.W_OK):
            _log.warning(f"Crash log thread name capturer disabled: Can't write to directory {self._webkit_thread_info_crashlog_dir}")
            return
        self._webkit_thread_info_crashlog_lockfile = os.path.join(self._webkit_thread_info_crashlog_dir, "thread-info-watch.lock")
        self._coredump_method, self._coredump_pattern, self._coredump_directory, self._coredump_directory_has_variable_format_specifiers = CrashLogUtils.determine_coredump_method_and_dir()
        if self._coredump_method == CoreDumpMethod.Unknown:
            _log.warning(f"Crash log thread name capturer disabled: Unknown coredump method configured. Please read the help")
            return
        if self._coredump_method == CoreDumpMethod.Abspath and not CrashLogUtils.core_pattern_has_pid_format_string(self._coredump_pattern):
            _log.warning(f"Crash log thread name capturer disabled: The %p format specifier is missing on core_pattern.")
            return
        if self._coredump_method == CoreDumpMethod.Abspath and self._coredump_directory_has_variable_format_specifiers:
            _log.warning(f"Crash log thread name capturer disabled: The coredump directory {self._coredump_directory} has variable format specifiers. Please define a simpler path (only %u %g %h %c specifiers allowed).")
            return
        if not self._coredump_directory or not os.path.isdir(self._coredump_directory) or not os.access(self._coredump_directory, os.R_OK | os.X_OK):
            _log.warning(f"Crash log thread name capturer disabled: Coredump directory not readable: {self._coredump_directory}")
            return
        if not CrashLogUtils.are_coredumps_enabled(self._coredump_method):
            _log.warning(f"Crash log thread name capturer disabled: coredump are not enabled. Please run: ulimit -c unlimited")
            return
        self._main_worker_thread = threading.Thread(target=self.watch_coredump_dir, daemon=True, name=f'watch-coredump-dir-{os.getpid()}')
        self._main_worker_thread.start()

    def _get_regex_for_core_pattern_pid_match(self):
        regex = None
        if self._coredump_method == CoreDumpMethod.Coredumpctl:
            # The format is core.<name>.<uid>.<machine-id>.<pid>.<timestamp>.zst
            regex = re.compile(r'core\..+?\.\d+\.[a-f0-9]+\.(?P<pid>\d+)\.\d+\.zst')
        elif self._coredump_method == CoreDumpMethod.Abspath:
            assert CrashLogUtils.core_pattern_has_pid_format_string(self._coredump_pattern)
            regex = re.compile(CrashLogUtils.core_pattern_to_regex(self._coredump_pattern, generate_regex_for_pid_match=True))
        return regex

    def watch_coredump_dir(self):
        if inotify_simple is None:
            _log.error(f"Can't start thread info name capturer: inotify_simple python module is not available. Please install it.")
            return
        # A lockfile is used to ensure only one watch_coredump_dir is active in the whole system in the given self._webkit_thread_info_crashlog_dir
        # If several test runners are launched then the other watch_coredump_dir threads will wait until the lockfile is released
        # This is because we only need one of this threads active at any moment in the whole system, having more than one can lead to race conditions when writing the thread info
        _log.debug(f"Crash log thread name capturer: watching coredumps directory {self._coredump_directory} and writing thread info to {self._webkit_thread_info_crashlog_dir}")
        with LockFile(self._webkit_thread_info_crashlog_lockfile):
            # lock forever on inotify IN_CREATE events in the given directory. Spawns a daemon thread per matching event.
            inotify = inotify_simple.INotify()
            # IN_CREATE fires the moment the kernel creates the file, which is the earliest possible moment before the dump is written.
            watch_flags = inotify_simple.flags.CREATE
            inotify.add_watch(self._coredump_directory, watch_flags)

            core_pattern_regex_for_pid = self._get_regex_for_core_pattern_pid_match()
            while True:
                # inotify.read() blocks until at least one event arrives.
                # It can return multiple events if they batched up.
                events = inotify.read()
                for event in events:
                    filename = event.name
                    if not filename:
                        continue
                    m = core_pattern_regex_for_pid.fullmatch(filename)
                    if not m:
                        continue
                    pid = m.group('pid')
                    # Spawn immediately to minimise latency
                    t = threading.Thread(target=self.handle_coredump, args=(pid,), daemon=True, name=f'coredump-worker-{pid}')
                    t.start()

    def read_thread_info(self, pid):
        task_dir = f'/proc/{pid}/task'
        try:
            tids = os.listdir(task_dir)
        except (FileNotFoundError, PermissionError):
            return None

        threads = {}
        for tid in tids:
            comm_path = f'{task_dir}/{tid}/comm'
            try:
                with open(comm_path, 'r') as f:
                    name = f.read().strip()
            except (FileNotFoundError, PermissionError):
                # Thread may have exited between listdir and open.
                name = '<gone>'
            threads[tid] = name

        return threads

    def handle_coredump(self, pid):
        # Called in a dedicated thread as soon as the coredump file is created.
        # Reads thread info and writes it to the threadinfo-${pid}.txt
        timestamp = datetime.datetime.now().isoformat(timespec='seconds')
        thread_info = self.read_thread_info(pid)
        output_path = os.path.join(self._webkit_thread_info_crashlog_dir, CrashLogUtils.get_thread_info_file_name(pid))
        _log.debug(f'Coredump detected for PID {pid}, saving "/proc/{pid}/task" info to "{output_path}" ...')
        try:
            with open(output_path, 'w') as f:
                f.write(f'# Thread info captured at {timestamp}\n')
                if thread_info is None:
                    if self._coredump_method == CoreDumpMethod.Coredumpctl and not CrashLogUtils.in_host_pid_namespace():
                        f.write(f'# ERROR: Impossible to get thread name info when using the coredumpctl method and running inside a pid namespace.\n')
                        f.write(f'#        Please disable the pid namespace or switch to using an abspath for kernel.core_pattern.\n')
                        f.write(f'# {CORE_PATTERN_RECOMMEND_COMMAND}\n')
                    else:
                        f.write(f'# ERROR: Process {pid} was already gone when we checked /proc/{pid}/task.\n')
                elif not thread_info:
                    # https://www.man7.org/linux/man-pages/man5/proc_pid_task.5.html
                    f.write(f'# WARNING: No threads found under /proc/{pid}/task (maybe main thread exited before the crash).\n')
                else:
                    f.write(f'# Number of threads: {len(thread_info)}\n')
                    f.write(f'# PID: {pid}\n')
                    f.write('\n')
                    f.write(f'{"TID":<12} {"Thread Name"}\n')
                    f.write(f'{"-"*12} {"-"*20}\n')
                    for tid, name in sorted(thread_info.items(), key=lambda x: int(x[0])):
                        f.write(f'{tid:<12} {name}\n')
        except OSError as e:
            _log.error(f'Failed to write {output_path}: {e}')


# This runs only once when the test suite start, it optionally cleans old cores and old thread-info files,
# and then starts the thread for generating the thread-info files
class GDBCrashLogStartupHandler(object):
    def __init__(self):
        self._coredump_method, self._coredump_pattern, self._coredump_directory, self._coredump_directory_has_variable_format_specifiers = CrashLogUtils.determine_coredump_method_and_dir()
        if os.environ.get(CrashLogEnvVars.core_dumps_autodelete, '0') == '1':
            # Even when we try to clean the coredumps as soon as those are processed sometimes there are uncleaned coredumps (usually caused by crashes not detected by this tooling),
            # so this opt-in helps to clean those anyway at startup time (used on the bots).
            if self._coredump_method == CoreDumpMethod.Abspath and os.path.isdir(self._coredump_directory):
                self.clean_old_coredumps()
            if os.path.isdir(CrashLogUtils.get_thread_data_dir()):
                self.clean_old_thread_info_files()
        # Try to create the dir if is not created already.
        if self._coredump_method == CoreDumpMethod.Abspath:
            try:
                os.makedirs(self._coredump_directory, exist_ok=True)
            except OSError as e:
                _log.error(f"Unable to create directory for coredumps {self._coredump_directory}: {e}")
        elif self._coredump_method == CoreDumpMethod.Coredumpctl and not CrashLogUtils.in_host_pid_namespace():
            _log.warning("Running inside a pid namespace different than the host. This causes non-reliable crash detection. Please disable the pid namespace or switch to using an abspath for kernel.core_pattern.")
        elif self._coredump_method == CoreDumpMethod.Unknown:
            _log.error("Directory for coredumps not defined. Please use coredumpctl or define an abspath at kernel.core_pattern")
        # Check if the debuginfod server is available to avoid hangs when generating the backtraces if the server is not responding
        self._check_debuginfod_servers()
        # Start the thread-info capturer
        ThreadNamesCrashLogCapturer()

    def _maybe_remove_file_if_old(self, path):
        # Define old as "more than 30 minutes"
        is_old = (time.time() - os.path.getmtime(path)) > 1800
        if is_old:
            _log.debug(f'Cleaning old file at: {path}')
            os.remove(path)
        else:
            _log.debug(f'Skipping non-old file at: {path}')

    def _is_debuginfod_server_available(self, url, timeout=5):
        try:
            r = requests.get(url.rstrip("/") + "/metrics", timeout=timeout)
        except requests.RequestException:
            return False

        if r.status_code != 200:
            return False

        required_markers = ('thread_busy', 'http_requests_total', 'groom', 'debuginfo')
        return all(marker in r.text for marker in required_markers)

    def _check_debuginfod_servers(self):
        if 'DEBUGINFOD_URLS' in os.environ:
            working_debuginfod_servers = []
            debuginfo_servers = os.environ.get('DEBUGINFOD_URLS').split()
            if not debuginfo_servers:
                return
            for server in debuginfo_servers:
                if server.startswith('file://'):
                    working_debuginfod_servers.append(server)
                elif server.startswith('http://') or server.startswith('https://'):
                    if self._is_debuginfod_server_available(server):
                        working_debuginfod_servers.append(server)
                    else:
                        _log.warning(f'Disabling debuginfod server {server} which seems offline.')
                else:
                    _log.warning(f'Disabling debuginfod server {server}: do not know how to handle it. Only http/https/file URIs are supported.')
            if working_debuginfod_servers:
                os.environ['DEBUGINFOD_URLS'] = ' '.join(working_debuginfod_servers)
            else:
                del os.environ['DEBUGINFOD_URLS']
            _log.debug(f'Environment variable DEBUGINFOD_URLS updated to "{os.environ.get("DEBUGINFOD_URLS", "")}"')

    def clean_old_coredumps(self):
        candidate_coredumps = [os.path.join(self._coredump_directory, f) for f in os.listdir(self._coredump_directory) if os.path.isfile(os.path.join(self._coredump_directory, f))]
        pattern_re = re.compile(CrashLogUtils.core_pattern_to_regex(self._coredump_pattern))
        candidate_coredumps = [f for f in candidate_coredumps if pattern_re.fullmatch(os.path.basename(f))]
        for coredump in candidate_coredumps:
            self._maybe_remove_file_if_old(coredump)

    def clean_old_thread_info_files(self):
        webkit_thread_info_crashlog_dir = CrashLogUtils.get_thread_data_dir()
        if os.path.isdir(webkit_thread_info_crashlog_dir):
            example_pid = '12345'
            prefix, suffix = CrashLogUtils.get_thread_info_file_name(example_pid).split(example_pid)
            for name in os.listdir(webkit_thread_info_crashlog_dir):
                path = os.path.join(webkit_thread_info_crashlog_dir, name)
                if os.path.isfile(path) and name.startswith(prefix) and name.endswith(suffix):
                    self._maybe_remove_file_if_old(path)


class GDBCrashLogGenerator(object):

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
        self._coredump_method, self._coredump_pattern, self._coredump_directory, self._coredump_directory_has_variable_format_specifiers = CrashLogUtils.determine_coredump_method_and_dir()
        self._webkit_thread_info_crashlog_dir = CrashLogUtils.get_thread_data_dir()
        self._effective_coredump_pid = None  # this is the pid of the coredump we will end use, ideally is the same than self.pid but not always
        self._regex_for_core_pattern = None
        if self._coredump_method == CoreDumpMethod.Abspath:
            self._regex_for_core_pattern = re.compile(CrashLogUtils.core_pattern_to_regex(self._coredump_pattern, generate_regex_for_pid_match=self._coredump_pattern_has_pid_format_string()))

    def _get_gdb_output(self, coredump_path):
        process_name = self._filesystem.join(os.path.dirname(str(self._path_to_driver())), self.name)
        # GDB may use quite a lot of CPU to generate a backtrace and also lot of RAM (specially on Debug builds)
        # So, to help to stabilize the system in situations were several crashes happen at the same time on the run
        # it is possible to configure the maximum number of maximum GDB processes that can run in parallel at a given time
        # The others requests to generate a backtrace will stay queued and wait for the current ones to finish.
        # If no limit is applied (the default) then gdb will run with the 'nice' wrapper to at least give it less priority
        # than other tester process to help avoiding timeouts. But this can starve the system RAM.
        # On the CI is recommended to set a limit.
        gdb_cmd_wrapper = [] if CrashLogUtils.get_gdb_concurrent_execution_limit() else ['nice']
        time_output_tmp = None
        time_output_content = ""
        if shutil.which('time'):
            time_output_tmp = CrashLogUtils.make_temp_path(suffix='.log')
            gdb_cmd_wrapper += ['time', '-v', f'--output={time_output_tmp}']
        gdb_cmd = ['gdb', '-iex', 'set debuginfod enabled on',
                   '-ex', 'thread apply all -ascending bt full 1024', '--batch', process_name, coredump_path]
        with CrashLogUtils.get_gdb_lock():
            _log.debug(f'Starting GDB process to generate a backtrace for {process_name} (pid {self._effective_coredump_pid})')
            proc = self._executive.popen(gdb_cmd_wrapper + gdb_cmd, stdin=None, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            stdout, stderr = proc.communicate()
        if proc.returncode != 0:
            stdout = (b'ERROR: The gdb process exited with non-zero return code %s\n\n' % str(proc.returncode).encode('utf8', 'ignore')) + stdout
        # Read time stats
        if time_output_tmp and os.path.isfile(time_output_tmp):
            with open(time_output_tmp, 'r') as f:
                lines = []
                for line in f:
                    line = line.strip()
                    # Skip lines with zero values: most rusage fields (average sizes, swaps, I/O, signals) are always zero on Linux
                    # because the kernel never implemented them -- they exist only for compatibility with BSD. See getrusage(2).
                    if line.endswith(': 0') and 'Exit status' not in line:
                        continue
                    lines.append(line)
                time_output_content = '\n'.join(lines)
            os.remove(time_output_tmp)
        return (stdout.decode('utf8', 'ignore'), stderr.decode('utf8', 'ignore'), time_output_content)

    def _coredump_pattern_has_pid_format_string(self):
        return CrashLogUtils.core_pattern_has_pid_format_string(self._coredump_pattern)

    def _pid_is_valid(self):
        return self.pid and str(self.pid).isnumeric()

    def _build_warning_msg(self, pid_found=False, name_found=False, last_found=False, nothing_found=False):
        if pid_found:
            return None
        if name_found:
            return f"Unable to find a coredump matching pid \"{self._pid_representation()}\".\n" +\
                   f"This coredump was selected by returning the last coredump matching the crashing program name \"{self.name}\".\n" +\
                   "There is a risk that the selected coredump doesn't match the crashing program.\n" +\
                   "It is recommended to retry running this test alone to see if it generates a coredump and a backtrace like this one.\n"
        if last_found:
            return f"Unable to find a coredump matching pid \"{self._pid_representation()}\" or matching the crashing program name \"{self.name}\".\n" +\
                   "This coredump was selected by simply returning the last one available.\n" +\
                   "There is a high risk that the selected coredump doesn't match the crashing program.\n" +\
                   "It is strongly recommended to retry running this test alone to see if it generates a coredump and a backtrace like this one.\n"
        if nothing_found:
            if CrashLogUtils.allow_unreliable_fallback_to_latest_coredump():
                nothing_found_err = f"Unable to find a coredump matching pid \"{self._pid_representation()}\" or matching the crashing program name \"{self.name}\".\n" +\
                                    "Unable to find any coredump"
                if self._coredump_method == CoreDumpMethod.Abspath and self._coredump_pattern_has_pid_format_string():
                    nothing_found_err += f" matching core_pattern \"{self._coredump_pattern}\""
                elif self._coredump_method == CoreDumpMethod.Coredumpctl:
                    nothing_found_err += " with coredumpctl"
                if self.newer_than:
                    nothing_found_err += f" newer than \"{self.newer_than}\""
                nothing_found_err += ".\n"
            else:
                nothing_found_err = f"Unable to find a coredump matching pid \"{self._pid_representation()}\"\n" +\
                                    f"Trying to match the coredump using other heuristics like matching by program name and/or timestamp was not tried,\n" +\
                                    f"because doing that is unreliable (specially with several parallel workers). If you want to try that for the next run\n" +\
                                    f"then export this environment variable before starting it: export {CrashLogEnvVars.allow_unreliable_fallback_to_latest_coredump}=1\n" +\
                                    f"And run the tests without parallel workers (if possible): export NUMBER_OF_PROCESSORS=1\n"
            return nothing_found_err
        raise RuntimeError("This should not be reached.")

    def _pick_most_recent(self, candidates, **warning_kwargs):
        if not candidates:
            return None
        coredump_match_candidate = max(candidates, key=os.path.getmtime)
        m = self._regex_for_core_pattern.fullmatch(os.path.basename(coredump_match_candidate))
        if not m:
            return None
        if self._coredump_pattern_has_pid_format_string():
            self._effective_coredump_pid = m.group('pid')
        return coredump_match_candidate, self._build_warning_msg(**warning_kwargs)

    def _get_coredump_path_with_core_pattern_method(self):
        can_match_by_pid = self._pid_is_valid() and self._coredump_pattern_has_pid_format_string()
        for try_number in range(2):
            candidate_coredumps = [os.path.join(self._coredump_directory, f) for f in os.listdir(self._coredump_directory) if os.path.isfile(os.path.join(self._coredump_directory, f))]
            pattern_re = re.compile(CrashLogUtils.core_pattern_to_regex(self._coredump_pattern))
            candidate_coredumps = [f for f in candidate_coredumps if pattern_re.fullmatch(os.path.basename(f)) and (not self.newer_than or os.path.getmtime(f) > self.newer_than)]

            # If we have a pid number then return the last file that matches the pid number ignoring the other format specifiers from core_pattern (if any)
            if can_match_by_pid:
                pid_matches = []
                for coredump in candidate_coredumps:
                    match = self._regex_for_core_pattern.fullmatch(os.path.basename(coredump))
                    if match and match.group('pid') == str(self.pid):
                        pid_matches.append(coredump)

                if pid_matches:
                    self._effective_coredump_pid = self.pid
                    return max(pid_matches, key=os.path.getmtime), self._build_warning_msg(pid_found=True)

                # If match by pid doesn't happen then sleep and retry once.
                # This may help if the system is under high stress and this code runs before the kernel starts to write the coredump to disk
                # which I'm not sure if that can even happen, but this also helps when two or more workers race to get the same core (one
                # matching by pid and another(s) by exe name), then the one matching by pid wins because the other has to sleep first.
                if try_number == 0:
                    time.sleep(1)
                    continue

                if not CrashLogUtils.allow_unreliable_fallback_to_latest_coredump():
                    return None, self._build_warning_msg(nothing_found=True)

            # Try the most recent file matching the program name. This is reached
            # either because can_match_by_pid is false, or because pid matching
            # failed and unreliable fallback was explicitly enabled.
            if self.name:
                program_matches = [f for f in candidate_coredumps if self.name in os.path.basename(f)]
                result = self._pick_most_recent(program_matches, name_found=True)
                if result:
                    return result

            # Same idea than above, if matching by pid is not possible then sleep 1 also on the first iter
            # so we have a better chance of a new core appearing on the second iteration.
            if try_number == 0:
                time.sleep(1)
                continue

            if not CrashLogUtils.allow_unreliable_fallback_to_latest_coredump():
                return None, self._build_warning_msg(nothing_found=True)

            # Reached this point just give the most recent file matching the pattern
            result = self._pick_most_recent(candidate_coredumps, last_found=True)
            if result:
                return result

            # Couldn't find anything.
            return None, self._build_warning_msg(nothing_found=True)

    def _coredumpctl_dump_core_for_pid(self, coredumpctl_args_for_pid, warnings):
        temp_file = CrashLogUtils.make_temp_path(dir=CrashLogUtils.get_temp_dir_for_coredumpctl_dumps(), suffix='.dump')
        warnings = warnings or ""
        failed = True
        try:
            self._executive.run_command(['coredumpctl', '-q', 'dump'] + coredumpctl_args_for_pid + ['--output', temp_file], return_stderr=True)
            failed = False
        except ScriptError as e:
            warnings += f"\nThe coredumpctl program returned an error when trying to dump the core for pid {self._effective_coredump_pid}:\n{e.output}\n"
        except OSError as e:
            warnings += f"\nFailed to launch coredumpctl for pid {self._effective_coredump_pid}:\n{e}\n"
        if failed:
            if os.path.isfile(temp_file):
                os.unlink(temp_file)
            temp_file = None
        return temp_file, warnings

    def _get_coredump_path_with_coredumpctl_method(self):
        coredumpctl_args_for_json = []
        if self.newer_than:
            coredumpctl_args_for_json.append('--since=@%f' % self.newer_than)

        list_cmd = ["coredumpctl", "-q", "--json=short"] + coredumpctl_args_for_json + ['list']

        can_match_by_pid = self._pid_is_valid() and CrashLogUtils.in_host_pid_namespace()
        # Retry up to 5 times with 1-second sleeps: systemd-coredump runs as a separate
        # userspace process, so by the time we ask, it may not yet have  ingested the dump.
        coredump_entries = []
        for try_number in range(5):
            if try_number != 0:
                time.sleep(1)
            try:
                list_output = self._executive.run_command(list_cmd, return_stderr=False)
            except ScriptError:
                # coredumpctl exits non-zero when there are no entries in the
                # requested time window; retry in case the dump shows up soon.
                continue
            except OSError as e:
                warnings = self._build_warning_msg(nothing_found=True)
                warnings += f"\nFailed to launch coredumpctl:\n{e}\n"
                return None, warnings

            try:
                coredump_entries = json.loads(list_output)
            except json.JSONDecodeError as e:
                warnings = self._build_warning_msg(nothing_found=True)
                warnings += f"\nFailed to decode coredumpctl JSON output:\n{e}\n"
                return None, warnings

            if can_match_by_pid:
                if any(str(e.get("pid")) == str(self.pid) for e in coredump_entries):
                    break
            else:
                if not self.name:
                    break  # nothing to wait for
                # If we don't have pid or we run on a pid namespace then we can only match by name
                if any(os.path.basename(e.get("exe") or "") == self.name for e in coredump_entries):
                    break

        if not coredump_entries:
            return None, self._build_warning_msg(nothing_found=True)

        # 1. Exact pid match
        if can_match_by_pid:
            pid_match = next((e for e in coredump_entries if str(e.get("pid")) == str(self.pid)), None)
            if pid_match:
                self._effective_coredump_pid = self.pid
                args = coredumpctl_args_for_json + [f"{self.pid}"]
                return self._coredumpctl_dump_core_for_pid(args, self._build_warning_msg(pid_found=True))

        # 2. Match by program name only when:
        #    - pid matching is impossible, or
        #    - the user explicitly enabled unreliable fallback.
        if self.name and (not can_match_by_pid or CrashLogUtils.allow_unreliable_fallback_to_latest_coredump()):
            program_matches = [e for e in coredump_entries if self.name == os.path.basename(e.get("exe") or "")]
            if program_matches:
                entry = max(program_matches, key=lambda e: e["time"])
                self._effective_coredump_pid = entry["pid"]
                args = coredumpctl_args_for_json + [f"{entry['pid']}"]
                return self._coredumpctl_dump_core_for_pid(args, self._build_warning_msg(name_found=True))

        if not CrashLogUtils.allow_unreliable_fallback_to_latest_coredump():
            return None, self._build_warning_msg(nothing_found=True)

        # 3. Latest coredump is always unreliable, so only when explicitly enabled unreliable fallback
        entry = max(coredump_entries, key=lambda e: e["time"])
        self._effective_coredump_pid = entry["pid"]
        args = coredumpctl_args_for_json + [f"{entry['pid']}"]
        return self._coredumpctl_dump_core_for_pid(args, self._build_warning_msg(last_found=True))

    def _get_coredump_path(self):
        if self._coredump_method == CoreDumpMethod.Abspath and not self._coredump_directory_has_variable_format_specifiers:
            return self._get_coredump_path_with_core_pattern_method()
        elif self._coredump_method == CoreDumpMethod.Coredumpctl:
            return self._get_coredump_path_with_coredumpctl_method()
        return None, self._build_warning_msg(nothing_found=True)

    def _get_help_message(self):
        coredumps_enabled = CrashLogUtils.are_coredumps_enabled(self._coredump_method)
        coredumps_enabled_and_unlimited = CrashLogUtils.are_coredumps_enabled_and_unlimited(self._coredump_method)
        tip_msg = "To enable crash logs:\n\n"
        hmsg = f"Coredump for pid {self._pid_representation()} not found.\n{tip_msg}"
        if not coredumps_enabled:
            hmsg += "    - Enable core dumps: ulimit -c unlimited\n"
        elif not coredumps_enabled_and_unlimited:
            hmsg += "    - You have coredumps enabled but not unlimited. Please rise the limit: ulimit -c unlimited\n"
        if self._coredump_method == CoreDumpMethod.Unknown:
            hmsg += f"    - Either install coredumpctl or execute this command: {CORE_PATTERN_RECOMMEND_COMMAND}\n\n" +\
                    f"Note: If you opt for the second method you can export {CrashLogEnvVars.core_dumps_autodelete}=1 to automatically clean coredumps after processing those.\n" +\
                    "      With the first method (coredumpctl) raw coredumps are cleaned automatically and only the compressed ones remain available.\n\n"
        elif self._coredump_method == CoreDumpMethod.Abspath:
            if not self._coredump_pattern_has_pid_format_string():
                hmsg += "    - Please fix the file pattern of your core_pattern. It should contain the \"%p\" format specifier and is also recommended to include \"%f\".\n" +\
                        f"      Example: {CORE_PATTERN_RECOMMEND_COMMAND} \n\n"
            if self._coredump_directory_has_variable_format_specifiers:
                hmsg += "    - Please fix the directory pattern of your core_pattern. Only fixed format specifiers are allowed for the directories (only %u %g %h %c specifiers allowed).\n" +\
                        f"      Example: {CORE_PATTERN_RECOMMEND_COMMAND} \n\n"
            elif not os.path.isdir(self._coredump_directory) or not os.access(self._coredump_directory, os.W_OK | os.R_OK | os.X_OK):
                hmsg += f"    - Please fix the directory {self._coredump_directory} : Either the directory is missing or the current user doesn't have read & write access.\n"
            else:
                cd_disk_usage = shutil.disk_usage(self._coredump_directory)
                cd_free_disk_gb = cd_disk_usage.free / (1024 ** 3)
                if cd_free_disk_gb <= 20:
                    hmsg += f"    - Please check the available disk space at {self._coredump_directory}:\n" + \
                            f"      Only {cd_free_disk_gb} GB free are there. This may not be enough, specially for Debug builds.\n"
        elif self._coredump_method == CoreDumpMethod.Coredumpctl:
            hmsg += "    - Please check that the current user has permissions to execute coredumpctl and to dump coredumps from it.\n"
            coredumpctl_dumps_tmpdir = CrashLogUtils.get_temp_dir_for_coredumpctl_dumps()
            cd_disk_usage = shutil.disk_usage(self._coredump_directory)
            cd_free_disk_pct = cd_disk_usage.free / cd_disk_usage.total * 100
            if cd_free_disk_pct <= 15:
                hmsg += f"    - Please check the available disk space at {self._coredump_directory}:\n" +\
                        f"      The 'KeepFree=' default value of systemd-coredump requires more than 15% free and the free disk space is only a {cd_free_disk_pct:.2f}% there.\n"
            td_disk_usage = shutil.disk_usage(coredumpctl_dumps_tmpdir)
            td_free_disk_gb = td_disk_usage.free / (1024 ** 3)
            if td_free_disk_gb <= 20:
                hmsg += f"    - Please check the available disk space at {coredumpctl_dumps_tmpdir}:\n" +\
                        f"      Only {td_free_disk_gb} GB free are there. This may not be enough, specially for Debug builds.\n"
        if hmsg.endswith(tip_msg):
            # If we can't find a cause for the missing coredump then replace the tip header with a descriptive error.
            hmsg = hmsg.replace(tip_msg, 'No coredump was generated. Environment seems correctly configured for coredump generation. Cause unknown.\n')
        return hmsg

    def _get_thread_info_for_effective_pid(self):
        if self._effective_coredump_pid:
            thread_info_file = os.path.join(self._webkit_thread_info_crashlog_dir, CrashLogUtils.get_thread_info_file_name(self._effective_coredump_pid))
            if os.path.isfile(thread_info_file):
                with open(thread_info_file, 'r') as f:
                    thread_info_content = f.read()
                # Clean it: we only need it once
                os.remove(thread_info_file)
                return thread_info_content
            return f"Thread names not available: could not find thread naming info for pid {self._effective_coredump_pid}.\n" +\
                   f"The thread_info_file was not found at {thread_info_file}\n"
        return f"Thread names not available: could not find the effective pid for the coredump."

    def _pid_representation(self):
        return str(self.pid or '<unknown>')

    def _filter_with_cppfilt(self, text):
        cppfilt_proc = self._executive.popen(['c++filt'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        text = cppfilt_proc.communicate(string_utils.encode(text))[0]
        text = string_utils.decode(text, errors='ignore')
        return text

    def _underscore_header(self, header, first_header=False):
        hstr = "" if first_header else "\n\n"
        underscores = '-' * len(header)
        hstr += f"{underscores}\n{header}\n{underscores}\n"
        return hstr

    def _generate_simplified_backtrace(self, gdb_backtrace, thread_info_data):
        # GDB "Thread 1" in a post-mortem coredump backtrace is always the crashing thread, because the Linux kernel
        # writes the NT_PRSTATUS note of the crashing thread first in fs/binfmt_elf.c, and GDB numbers the threads
        # in the order it reads those notes. See https://github.com/WebKit/WebKit/pull/65292#discussion_r3278153037
        simplified_backtrace = []
        print_frames = False
        for line in gdb_backtrace.splitlines():
            if print_frames and re.match(r'#\d+ ', line):
                simplified_backtrace.append(line)
            if line.startswith('Core was generated by') or line.startswith('Program terminated with'):
                simplified_backtrace.append(line)
            if line.startswith("Thread ") and not line.startswith("Thread 1 "):
                print_frames = False
            if line.startswith("Thread 1 "):
                print_frames = True
                thread_name = None
                pid = None
                tid = None
                if thread_info_data:
                    # LWP means Light Weight Process, is the same than TID.
                    m = re.search(r'LWP (\d+)', line)
                    if m:
                        tid = m.group(1)
                        for thread_info_line in thread_info_data.splitlines():
                            if thread_info_line.startswith('# PID: '):
                                pid = thread_info_line.split()[2]
                            if thread_info_line.startswith(f'{tid} '):
                                thread_name = thread_info_line.split(None, 1)[1]
                                # safe to break here, the pid value is always above than the TID ones
                                break
                crash_thread_header = "Crashed thread:"
                if thread_name:
                    crash_thread_header += f" {thread_name} (PID={pid}, LWP/TID={tid})"
                    if pid == tid:
                        crash_thread_header += " (main thread)"
                    else:
                        crash_thread_header += " (not main thread)"
                simplified_backtrace.append(crash_thread_header)
                simplified_backtrace.append(line)

        if not simplified_backtrace:
            return None
        simplified_backtrace.append('')
        return '\n'.join(simplified_backtrace)

    def generate_crash_log(self, test_stdout, test_stderr):
        timestamp_start = time.monotonic()
        time_output = ""
        gdb_stderr = ""
        gdb_backtrace = ""

        coredump_path, warnings_found = self._get_coredump_path()
        coredump_path_was_found = coredump_path and os.path.isfile(coredump_path)
        if coredump_path_was_found:
            coredump_hr_size = CrashLogUtils.human_readable_size(coredump_path)
            should_delete_coredump = self._coredump_method == CoreDumpMethod.Coredumpctl or os.environ.get(CrashLogEnvVars.core_dumps_autodelete, '0') == '1'
            try:
                gdb_backtrace, gdb_stderr, time_output = self._get_gdb_output(coredump_path)
            finally:
                if should_delete_coredump:
                    try:
                        os.remove(coredump_path)
                    except OSError as e:
                        _log.error(f'Could not remove {coredump_path}: {e}')

        if test_stderr:
            test_stderr = self._filter_with_cppfilt(test_stderr)

        if gdb_stderr:
            gdb_stderr = '\n'.join(line for line in gdb_stderr.splitlines() if line.strip())
            gdb_stderr = self._filter_with_cppfilt(gdb_stderr)

        if not gdb_backtrace:
            gdb_backtrace = self._get_help_message()

        thread_info_data = self._get_thread_info_for_effective_pid()

        seconds_elapsed = time.monotonic() - timestamp_start
        # All data gathered, now print it
        crash_log = self._underscore_header(f"Crash log for {self.name} (pid {self._pid_representation()})", first_header=True)
        crash_log += f'Total crash log generation took: {seconds_elapsed:.2f}s\n'

        # If we got a coredump for another pid than self.pid or self.pid is unknown then give some extra warnings
        if coredump_path_was_found:
            if self._coredump_method == CoreDumpMethod.Abspath:
                crash_log += f"Coredump file: {coredump_path}\n"
                crash_log += f"Coredump size: {coredump_hr_size}\n"
            elif self._coredump_method == CoreDumpMethod.Coredumpctl:
                crash_log += f"Coredump file dump command: coredumpctl dump {self._effective_coredump_pid} --output {coredump_path}\n"
                crash_log += f"Coredump uncompressed size: {coredump_hr_size}\n"

            if str(self.pid) != str(self._effective_coredump_pid):
                if self._coredump_method == CoreDumpMethod.Abspath and not self._coredump_pattern_has_pid_format_string():
                    crash_log += f"\nWARNING: Cannot verify which pid this coredump belongs to: the kernel.core_pattern lacks the %p format specifier.\n"
                else:
                    crash_log += f"\nWARNING: The selected coredump to generate this backtrace was generated for pid {self._effective_coredump_pid}.\n"
                s = ' ' * len('WARNING: ')
                if self._pid_is_valid():
                    if self._coredump_method == CoreDumpMethod.Coredumpctl and not CrashLogUtils.in_host_pid_namespace():
                        crash_log += f"{s}This is because the test suite runs on a different namespace than the host one where coredumpctl runs.\n"
                        crash_log += f"{s}TIP: Switch to the abspath method (raw coredump files) which is more reliable in this case:\n"
                        crash_log += f"{s}    {CORE_PATTERN_RECOMMEND_COMMAND}\n"
                    else:
                        crash_log += f"{s}This is because a coredump for pid {self._pid_representation()} couldn't be found and this one was selected.\n"
                else:
                    crash_log += f"{s}This is because the pid for the crashing test was not valid ({self._pid_representation()}) and this coredump was selected.\n"
                crash_log += f"{s}This difference introduces the possibility that the selected coredump was not created by this crashing test.\n\n"

        if warnings_found:
            crash_log += f"\nWARNING: {warnings_found}\n"
        if test_stdout:
            crash_log += self._underscore_header(f"TEST STDOUT")
            crash_log += test_stdout
        if test_stderr:
            crash_log += self._underscore_header(f"TEST STDERR")
            crash_log += test_stderr
        simplified_backtrace = self._generate_simplified_backtrace(gdb_backtrace, thread_info_data)
        if simplified_backtrace:
            crash_log += self._underscore_header(f"SIMPLIFIED BACKTRACE")
            crash_log += simplified_backtrace
        crash_log += self._underscore_header(f"THREAD NAMING INFO")
        crash_log += thread_info_data
        crash_log += self._underscore_header(f"GDB FULL BACKTRACE")
        crash_log += f'{gdb_backtrace}\n'
        if gdb_stderr:
            crash_log += self._underscore_header(f"GDB STDERR")
            crash_log += f'{gdb_stderr}\n'
        if time_output:
            crash_log += self._underscore_header(f"GDB PROCESS STATS")
            crash_log += time_output

        return (test_stderr, crash_log)

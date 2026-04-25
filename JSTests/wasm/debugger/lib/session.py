"""
DebugSession — owns one JSC process + one LLDB process for a single test.

Thread model:
  - 4 daemon threads drain the 4 subprocess streams (prevent pipe deadlock)
  - LLDB stdout+stderr lines are pushed to self._q
  - cmd() reads self._q synchronously in the calling thread — no per-call threads
  - No shared state between sessions → parallel tests are isolated by construction

Connection handshake:
  1. JSC starts (runs freely — no initial pause).
  2. JS loads all WASM modules, then prints "DEBUGGER_READY" to stdout.
  3. DebugSession detects "DEBUGGER_READY" via threading.Event on the JSC
     stdout reader thread, then (and only then) spawns LLDB.
  4. LLDB connects to the already-running JSC process; JSC stops immediately.
  5. _wait("Process 1 stopped") confirms the attach before execute() begins.

  Guarantee: all initial module-load notifications fire before LLDB attaches,
  so no unexpected "Process 1 stopped" events pollute the test command flow.
"""

import os
import queue
import re
import subprocess
import threading
import time
from pathlib import Path

# Root directory of the debugger test suite (contains test-wasm-debugger.py)
_TESTS_ROOT = Path(__file__).parent.parent

# Default wait patterns for commands that change process state.
# Tests that need different patterns pass them explicitly.
_DEFAULT_PATTERNS = {
    "c":                 ["Process 1 resuming"],
    "process interrupt": ["Process 1 stopped"],
    "n":                 ["Process 1 stopped"],
    "s":                 ["Process 1 stopped"],
    "si":                ["Process 1 stopped"],
    "fin":               ["Process 1 stopped"],
}


class DebugSession:
    """
    Manages one JSC + LLDB pair for a single test run.

    Usage:
        with DebugSession(test_file, port, jsc_path, lldb_path, env_vars) as session:
            session.cmd("b 0x4000000000000036")
            session.cmd("c")
            session.cmd("dis", ["->  0x4000000000000038"])
    """

    def __init__(self, test_file, port, jsc_path, lldb_path, env_vars=None,
                 extra_jsc_options=None, verbose=False, name=""):
        """
        Args:
            test_file:          Path relative to _TESTS_ROOT (e.g. "resources/wasm/call.js")
            port:               TCP port for the WASM debugger protocol
            jsc_path:           Absolute path to the jsc binary
            lldb_path:          Absolute path (or name) of lldb
            env_vars:           os.environ dict override (for DYLD_FRAMEWORK_PATH etc.)
            extra_jsc_options:  Additional JSC flags e.g. ["--useDollarVM=1"]
            verbose:            Print all subprocess output to stdout
            name:               Test name used as prefix in verbose output
        """
        self._q = queue.Queue()
        self._verbose = verbose
        self._name = name
        self._jsc_ready = threading.Event()
        self._jsc = None  # initialized before try so close() is always safe to call
        self._lldb = None

        # Build JSC command
        jsc_cmd = [str(jsc_path), f"--wasm-debugger={port}"]
        if extra_jsc_options:
            jsc_cmd.extend(extra_jsc_options)
        jsc_cmd.append(os.path.basename(test_file))

        # Set working directory to the resource subdirectory so relative paths
        # inside the JS test file resolve correctly.
        cwd = str(_TESTS_ROOT / os.path.dirname(test_file))

        try:
            # Step 1 — start JSC. It runs freely (no initial pause); JS loads all
            # WASM modules, then prints "DEBUGGER_READY" to signal readiness.
            self._jsc = subprocess.Popen(
                jsc_cmd,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                text=True, bufsize=1,
                cwd=cwd,
                env=env_vars,
            )

            # Start JSC reader threads now so stdout is drained and the
            # DEBUGGER_READY signal can be detected before LLDB is spawned.
            self._start_reader(self._jsc.stdout, "JSC", "stdout", to_queue=False,
                               ready_event=self._jsc_ready)
            self._start_reader(self._jsc.stderr, "JSC", "stderr", to_queue=False)

            # Step 2 — wait for JS to finish loading all modules.
            if not self._jsc_ready.wait(timeout=60.0):
                raise TimeoutError(
                    f"[{self._name}] JSC did not print DEBUGGER_READY within 60 s"
                )

            # Step 3 — all modules are loaded; connect LLDB now. Any module-load
            # notifications that fired before this point are irrelevant to LLDB.
            connect_cmd = f"process connect --plugin wasm connect://localhost:{port}"
            self._lldb = subprocess.Popen(
                [str(lldb_path), "-o", connect_cmd],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                text=True, bufsize=1,
            )

            self._start_reader(self._lldb.stdout, "LLDB", "stdout", to_queue=True)
            self._start_reader(self._lldb.stderr, "LLDB", "stderr", to_queue=True)

            # Step 4 — block until LLDB reports the initial attach stop.
            self._wait(["Process 1 stopped"], mode="any", timeout=60.0)
        except BaseException:
            self.close()
            raise

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def cmd(self, command, patterns=None, mode="all", timeout=60.0):
        """
        Send a command to LLDB and optionally wait for output patterns.

        Args:
            command:   LLDB command string
            patterns:  List of literal strings to match in LLDB output.
                       None  → use built-in default for this command (if any).
                       []    → send and return immediately (no waiting).
            mode:      "all" (default) — every pattern must appear.
                       "any"           — stop as soon as one pattern appears.
            timeout:   Seconds before raising TimeoutError.
        """
        # "process interrupt" is unreliable in batch/piped mode: LLDB's Halt() checks
        # GetPublicState() which reads PrivateStateThread::m_public_state — updated
        # asynchronously when ProcessEventData::DoOnRemoval() fires.  Process::Resume()
        # only updates m_public_run_lock synchronously, so Halt() can see stale
        # eStateStopped and return "Process is not running." without sending \x03 to
        # JSC.  This is an inherent LLDB race; do not call "process interrupt" in tests.
        if command.strip().lower() == "process interrupt":
            raise NotImplementedError(
                "'process interrupt' is unreliable in batch/piped LLDB mode — "
                "see cmd() source for details."
            )

        if self._verbose:
            print(f"[{self._name}][LLDB]> {command}")
        self._lldb.stdin.write(f"{command}\n")
        self._lldb.stdin.flush()

        if patterns is None:
            patterns = _DEFAULT_PATTERNS.get(command.strip().lower())
        if patterns:
            self._wait(patterns, mode, timeout)

    def close(self):
        """Kill LLDB and JSC and wait for them to exit."""
        for proc in (self._lldb, self._jsc):
            if proc is None:
                continue
            try:
                proc.kill()
            except OSError:
                pass
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                pass

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _wait(self, patterns, mode, timeout):
        """
        Read from self._q until all/any patterns match or timeout. Synchronous; no extra threads.
        """
        compiled = [re.compile(re.escape(p)) for p in patterns]
        matched = set()
        deadline = time.monotonic() + timeout

        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                unmatched = [patterns[i] for i in range(len(patterns)) if i not in matched]
                raise TimeoutError(
                    f"[{self._name}] Timed out waiting for: {unmatched}"
                )
            try:
                line = self._q.get(timeout=min(remaining, 0.1))
                if self._verbose:
                    print(f"[{self._name}][LLDB] {line}")
                for i, pat in enumerate(compiled):
                    if i not in matched and pat.search(line):
                        matched.add(i)
                if mode == "any" and matched:
                    return
                if mode == "all" and len(matched) == len(compiled):
                    return
            except queue.Empty:
                continue

    def _start_reader(self, stream, proc_name, kind, to_queue, ready_event=None):
        """Drain stream in a daemon thread; set ready_event when "DEBUGGER_READY" is seen."""
        def _read():
            for line in iter(stream.readline, ""):
                line = line.rstrip()
                if not line:
                    continue
                if self._verbose:
                    print(f"[{self._name}][{proc_name}][{kind}] {line}")
                if ready_event and "DEBUGGER_READY" in line:
                    ready_event.set()
                if to_queue:
                    self._q.put(line)

        threading.Thread(target=_read, daemon=True).start()

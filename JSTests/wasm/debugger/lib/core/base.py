"""
Base test case classes and LLDB pattern matching for WebAssembly debugger tests
"""

import os
import subprocess
import time
import threading
import queue
from typing import Optional, List, Union
from enum import Enum
import re

from ..runners.process_manager import ProcessManager
from .utils import Logger
from .environment import WebKitEnvironment


class PatternMatchMode(Enum):
    """Pattern matching modes for LLDB output monitoring"""

    ANY = "any"  # Complete when ANY pattern is matched
    ALL = "all"  # Complete when ALL patterns are matched


class PatternMatcher:
    """Enhanced pattern matcher for LLDB output with flexible matching modes"""

    def __init__(
        self,
        patterns: List[Union[str, re.Pattern]],
        mode: PatternMatchMode = PatternMatchMode.ANY,
    ):
        """
        Initialize pattern matcher

        Args:
            patterns: List of string patterns or compiled regex patterns
            mode: PatternMatchMode.ANY (any pattern matches) or PatternMatchMode.ALL (all patterns must match)
        """
        self.patterns = []
        self.mode = mode
        self.matched_patterns = set()

        # Convert string patterns to compiled regex patterns
        for pattern in patterns:
            if isinstance(pattern, str):
                # Escape special regex characters for literal string matching
                escaped_pattern = re.escape(pattern)
                self.patterns.append(re.compile(escaped_pattern))
            elif isinstance(pattern, re.Pattern):
                self.patterns.append(pattern)
            else:
                raise ValueError(
                    f"Pattern must be string or compiled regex, got {type(pattern)}"
                )

    def check_line(self, line: str) -> bool:
        """
        Check if a line matches the pattern criteria

        Args:
            line: Line of text to check

        Returns:
            True if completion criteria are met, False otherwise
        """
        line_matched_any = False

        for i, pattern in enumerate(self.patterns):
            if pattern.search(line):
                self.matched_patterns.add(i)
                line_matched_any = True

                # For ANY mode, return immediately on first match
                if self.mode == PatternMatchMode.ANY:
                    return True

        # For ALL mode, check if all patterns have been matched
        if self.mode == PatternMatchMode.ALL:
            return len(self.matched_patterns) == len(self.patterns)

        return False

    def reset(self):
        """Reset the matcher state"""
        self.matched_patterns.clear()

    def get_matched_patterns(self) -> List[int]:
        """Get indices of patterns that have been matched"""
        return sorted(list(self.matched_patterns))

    def is_complete(self) -> bool:
        """Check if matching is complete based on mode"""
        if self.mode == PatternMatchMode.ANY:
            return len(self.matched_patterns) > 0
        else:  # ALL mode
            return len(self.matched_patterns) == len(self.patterns)


class LLDBMonitoringResult:
    """Result container for LLDB monitoring operations"""

    def __init__(self):
        self.success = False
        self.matched_patterns = []
        self.output_lines = []
        self.completion_reason = ""
        self.timeout = False
        self.error = ""


class TestResult:
    """Test result container"""

    def __init__(self, name: str):
        self.name = name
        self.success = False
        self.error_message = ""
        self.start_time = time.time()
        self.end_time = None

    def mark_success(self):
        """Mark test as successful"""
        self.success = True
        self.end_time = time.time()

    def mark_failure(self, error_message: str):
        """Mark test as failed with error message"""
        self.success = False
        self.error_message = error_message
        self.end_time = time.time()

    def duration(self) -> float:
        """Get test duration in seconds"""
        if self.end_time:
            return self.end_time - self.start_time
        return time.time() - self.start_time


class BaseTestCase:
    """Base class for all WebAssembly debugger test cases"""

    def __init__(self, build_config: str = None, port: int = None):
        self.name = self.__class__.__name__
        self.logger = Logger()

        # Initialize environment with test directory path
        from pathlib import Path

        test_dir = Path(__file__).parent.parent.parent / "test-wasm-debugger.py"
        self.env = WebKitEnvironment(test_dir, build_config)

        # Port allocation: use provided port or default for sequential execution
        self.current_port = port if port is not None else 12340
        self.process_manager = ProcessManager(self.current_port)

        # Direct process references
        self.debugger_process: Optional[subprocess.Popen] = None
        self.lldb_process: Optional[subprocess.Popen] = None

        # Verbose WebAssembly debugger flag (separate from test framework verbose)
        self._verbose_wasm_debugger = False

        # Thread-safe LLDB output storage and pattern matching
        self._lldb_output_lock = threading.Lock()
        self._lldb_output = []

        # Queue-based synchronization for reliable pattern matching
        self._lldb_line_queue = queue.Queue()
        self._stop_monitoring = threading.Event()

        # Process and thread tracking (injected by parallel runner)
        self._process_tracker = None
        self._worker_tid = None

    def setup(self):
        """Setup method called before test execution"""
        self.logger.header(f"Setting up test: {self.name}")

    def teardown(self):
        """Cleanup method called after test execution"""
        self.logger.header(f"Tearing down test: {self.name}")

        # Stop monitoring and clear queue
        self._stop_monitoring.set()

        # Gracefully disconnect LLDB before cleanup
        self._disconnect_lldb_gracefully()

        # Clear queue without task_done() since we're not using join()
        while not self._lldb_line_queue.empty():
            try:
                self._lldb_line_queue.get_nowait()
            except queue.Empty:
                break

        # Clean up processes if they exist
        if self.debugger_process or self.lldb_process:
            self.process_manager.cleanup_processes()

        # Reset process references
        self.debugger_process = None
        self.lldb_process = None

    def run(self) -> TestResult:
        """Run the test case"""
        result = TestResult(self.name)
        try:
            self.setup()
            self.execute()
            result.mark_success()
            self.logger.success(f"Test {self.name} passed ({result.duration():.2f}s)")
        except Exception as e:
            result.mark_failure(str(e))
            self.logger.error(
                f"Test {self.name} failed ({result.duration():.2f}s): {e}"
            )
        finally:
            self.teardown()
        return result

    def execute(self):
        """Override this method in subclasses to implement test logic"""
        raise NotImplementedError("Subclasses must implement execute() method")

    def set_verbose_wasm_debugger(self, verbose: bool):
        """Set verbose WebAssembly debugger flag"""
        self._verbose_wasm_debugger = verbose

    def set_process_tracker(self, tracker, worker_tid: int):
        """Set process tracker for monitoring (injected by parallel runner)"""
        self._process_tracker = tracker
        self._worker_tid = worker_tid

    def start_debugger(self, test_file: str) -> bool:
        """
        Start the WebAssembly debugger server

        Args:
            test_file: Path to the test file (JavaScript or WebAssembly) to debug

        Returns:
            True if debugger started successfully, False otherwise
        """
        # Use the pre-allocated port (no need to allocate)
        if not self.current_port:
            raise Exception("No port allocated for this test case")

        # Build command
        jsc_path = self.env.get_jsc_path()
        if not jsc_path:
            raise Exception("JSC executable not found")

        cmd = [jsc_path, f"--wasm-debug={self.current_port}"]

        cmd.append(test_file)

        self.logger.verbose(f"Starting debugger on port {self.current_port}")
        self.logger.verbose(f"Command: {' '.join(cmd)}")

        try:
            # Start debugger process with proper environment and working directory
            test_dir = os.path.dirname(
                os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
            )  # Go up to test directory (from lib/core/base.py to debugger/)

            # If test_file contains a directory (like "resources/c-wasm/add/main.js"), set working directory to that subdirectory
            if "/" in test_file:
                test_subdir = os.path.dirname(test_file)
                working_dir = os.path.join(test_dir, test_subdir)
                # Update the command to use just the filename
                test_filename = os.path.basename(test_file)
                cmd = [jsc_path, f"--wasm-debug={self.current_port}"]

                cmd.append(test_filename)
            else:
                working_dir = test_dir

            self.logger.verbose(f"Working directory: {working_dir}")
            self.logger.verbose(
                f"Environment variables: VM={self.env.env.get('VM')}, DYLD_FRAMEWORK_PATH={self.env.env.get('DYLD_FRAMEWORK_PATH')}"
            )
            self.logger.verbose("Starting debugger subprocess...")

            self.debugger_process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1,
                universal_newlines=True,
                cwd=working_dir,  # Set working directory appropriately
                env=self.env.env,  # Use the WebKit environment with DYLD_FRAMEWORK_PATH
            )

            self.logger.verbose(
                f"Debugger process started with PID: {self.debugger_process.pid}"
            )

            # Track JSC process
            if self._process_tracker and self._worker_tid:
                self._process_tracker.track_process(
                    self._worker_tid, "JSC", self.debugger_process.pid
                )

            # Start debugger output monitoring thread
            self.logger.verbose("Starting debugger output monitoring thread...")
            self._start_debugger_monitoring()
            self.logger.verbose(
                f"Debugger started successfully on port {self.current_port}"
            )
            return True

        except Exception as e:
            self.logger.error(f"Failed to start debugger: {e}")
            if self.debugger_process:
                self.debugger_process.terminate()
                self.debugger_process = None
            return False

    def start_lldb(self, connection_timeout: float = 10.0) -> bool:
        """
        Start LLDB and connect to the debugger using WebAssembly plugin

        Args:
            connection_timeout: Maximum time to wait for connection and stop detection

        Returns:
            True if LLDB started and connected successfully, False otherwise
        """
        if not self.current_port:
            raise Exception("No debugger port allocated")

        # Use environment detection for LLDB path
        lldb_path = self.env.get_lldb_path()
        if not lldb_path:
            raise Exception("LLDB executable not found")

        self.logger.verbose(f"Starting LLDB and connecting to port {self.current_port}")
        self.logger.verbose(f"Using LLDB at: {lldb_path}")

        try:
            # Use LLDB with direct WebAssembly connection command
            connect_cmd = (
                f"process connect --plugin wasm connect://localhost:{self.current_port}"
            )

            self.logger.verbose("Starting LLDB with WebAssembly connection...")
            self.logger.verbose(f"Connection command: {connect_cmd}")

            # Start LLDB process with the connection command
            self.lldb_process = subprocess.Popen(
                [
                    lldb_path,
                    # FIXME: Should remove these two once Swift LLDB step over issue is fixed
                    "-o",
                    "settings set stop-line-count-before 0",  # FIXME: Disable showing lines before
                    "-o",
                    "settings set stop-line-count-after 0",  # FIXME: Disable showing lines after
                    "-o",
                    connect_cmd,
                ],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1,
                universal_newlines=True,
            )
            self.logger.verbose(
                f"LLDB process started with PID: {self.lldb_process.pid}"
            )

            # Track LLDB process
            if self._process_tracker and self._worker_tid:
                self._process_tracker.track_process(
                    self._worker_tid, "LLDB", self.lldb_process.pid
                )

            # Start output monitoring thread
            self.logger.verbose("Starting LLDB output monitoring thread...")
            self._start_lldb_monitoring()

            # Wait for stop detection with timeout
            self.logger.verbose(
                f"Waiting for LLDB stop detection (timeout: {connection_timeout}s)..."
            )
            if self.wait_for_lldb_stop(connection_timeout):
                # Register processes with manager
                self.process_manager.register_processes(
                    self.debugger_process,
                    self.lldb_process,
                    self.name,
                )

                self.logger.success(
                    f"LLDB connected to debugger on port {self.current_port} using WebAssembly plugin"
                )
                return True
            else:
                self.logger.error(
                    f"❌ LLDB connection failed - no stop detected within {connection_timeout}s"
                )
                if self.lldb_process:
                    self.lldb_process.terminate()
                    self.lldb_process = None
                return False

        except Exception as e:
            self.logger.error(f"Failed to start LLDB: {e}")
            if self.lldb_process:
                self.lldb_process.terminate()
                self.lldb_process = None
            return False

    def setup_debugging_session(self, test_file: str) -> bool:
        """
        Setup complete debugging session (debugger + LLDB)

        Args:
            test_file: Path to the test file (JavaScript or WebAssembly) to debug

        Returns:
            True if both debugger and LLDB started successfully, False otherwise
        """
        self.logger.header(f"Setting up debugging session for {self.name}")

        # Start debugger
        if not self.start_debugger(test_file):
            return False

        # Start LLDB
        if not self.start_lldb():
            return False

        self.logger.success(f"Debugging session ready for {self.name}")
        return True

    def send_lldb_command_with_patterns(
        self,
        command: str,
        patterns: List[Union[str, re.Pattern]] = None,
        mode: PatternMatchMode = PatternMatchMode.ALL,
        timeout: float = 5.0,
    ) -> LLDBMonitoringResult:
        """
        Send command to LLDB and wait for specific patterns in the output

        Args:
            command: LLDB command to send
            patterns: List of patterns to wait for (if None, uses command-specific patterns)
            mode: PatternMatchMode.ANY or PatternMatchMode.ALL
            timeout: Maximum time to wait for completion

        Returns:
            LLDBMonitoringResult with command execution status
        """
        if not self.lldb_process:
            result = LLDBMonitoringResult()
            result.error = "No LLDB process available"
            return result

        # Use command-specific patterns if none provided
        if patterns is None:
            patterns = self._get_command_completion_patterns(command)

        try:
            # Send the command
            self.logger.verbose(
                f"Sending LLDB command with pattern monitoring: {command}"
            )

            self.lldb_process.stdin.write(f"{command}\n")
            self.lldb_process.stdin.flush()

            # Wait for completion using pattern matching
            result = self.wait_for_lldb_patterns(patterns, mode, timeout)
            result.output_lines = [
                line for line in result.output_lines if line.strip()
            ]  # Filter empty lines

            return result

        except Exception as e:
            result = LLDBMonitoringResult()
            result.error = f"Failed to send LLDB command: {e}"
            self.logger.error(result.error)
            return result

    def send_lldb_command_or_raise(
        self, command: str, patterns=None, mode=PatternMatchMode.ALL, timeout=5.0
    ):
        """Send LLDB command with patterns and raise exception on failure"""
        result = self.send_lldb_command_with_patterns(command, patterns, mode, timeout)
        if not result.success:
            raise Exception(f"Command '{command}' failed: {result.error}")
        return result

    def setup_debugging_session_or_raise(self, test_file: str):
        """Setup complete debugging session and raise exception on failure"""
        if not self.setup_debugging_session(test_file):
            raise Exception("Session setup failed")

    def wait_for_lldb_stop(self, timeout: float = 10.0) -> bool:
        """Wait for LLDB to detect a stop (breakpoint hit, etc.)"""
        self.logger.verbose(f"Waiting for LLDB stop (timeout: {timeout}s)")
        result = self.wait_for_lldb_patterns(
            ["Process 1 stopped"], PatternMatchMode.ANY, timeout
        )

        if result.success:
            self.logger.success("LLDB stop detected")
            return True
        else:
            self.logger.warning(f"⏰ Timeout waiting for LLDB stop after {timeout}s")
            return False

    def wait_for_lldb_patterns(
        self,
        patterns: List[Union[str, re.Pattern]],
        mode: PatternMatchMode = PatternMatchMode.ANY,
        timeout: float = 10.0,
    ) -> LLDBMonitoringResult:
        """
        Wait for LLDB output to match specific patterns using queue-based synchronization

        Args:
            patterns: List of patterns to match (strings or compiled regex)
            mode: PatternMatchMode.ANY (any pattern) or PatternMatchMode.ALL (all patterns)
            timeout: Maximum time to wait in seconds

        Returns:
            LLDBMonitoringResult with success status and matched patterns
        """
        self.logger.verbose(
            f"Waiting for LLDB Patterns: {[str(p) for p in patterns]} (mode: {mode.value}, timeout: {timeout}s)"
        )

        # Create pattern matcher
        pattern_matcher = PatternMatcher(patterns, mode)
        result = LLDBMonitoringResult()

        # Pattern matching with simplified worker thread
        match_found = threading.Event()

        def pattern_worker():
            """Simple worker thread for pattern matching"""
            while not self._stop_monitoring.is_set():
                try:
                    line = self._lldb_line_queue.get(timeout=0.1)
                    result.output_lines.append(line)

                    if pattern_matcher.check_line(line):
                        result.success = True
                        result.matched_patterns = pattern_matcher.get_matched_patterns()
                        result.completion_reason = (
                            f"Patterns matched in {mode.value} mode"
                        )
                        match_found.set()
                        return

                except queue.Empty:
                    continue
                except Exception as e:
                    result.error = str(e)
                    match_found.set()
                    return

        # Start worker and wait for completion
        worker_thread = threading.Thread(target=pattern_worker, daemon=True)
        worker_thread.start()

        pattern_matched = match_found.wait(timeout)

        # Clean shutdown
        self._stop_monitoring.set()
        worker_thread.join(timeout=0.5)
        self._stop_monitoring.clear()

        # Finalize result
        if pattern_matched and result.success:
            # Enhanced logging: show actual patterns that matched
            matched_pattern_details = []
            for idx in result.matched_patterns:
                if idx < len(patterns):
                    pattern_str = str(patterns[idx])
                    matched_pattern_details.append(f"[{idx}]: '{pattern_str}'")

            self.logger.verbose(
                f"🎯 LLDB patterns matched ({mode.value} mode): {', '.join(matched_pattern_details)}"
            )
        else:
            result.success = False
            result.timeout = True
            result.completion_reason = f"Timeout after {timeout}s"
            self.logger.warning(
                f"⏰ Timeout waiting for LLDB patterns after {timeout}s"
            )

        return result

    def _get_command_completion_patterns(self, command: str) -> List[str]:
        """Get completion patterns for LLDB commands"""
        cmd = command.strip().lower()

        if cmd == "c":
            return ["Process 1 resuming"]
        elif cmd in ["n", "s", "si", "fin", "process interrupt"]:
            return ["Process 1 stopped"]
        elif cmd in ["dis", "disass"]:
            return ["->.*:"]
        elif cmd.startswith("b "):
            return ["Breakpoint"]
        elif cmd.startswith(("frame variable", "p ")):
            return ["=", "\\(lldb\\)"]
        else:
            raise ValueError(f"No completion patterns defined for command: '{command}'")

    def _start_process_monitoring(self, process, process_name, pattern_matching=False):
        """Simple process monitoring with thread tracking"""

        def read_stream(stream, stream_name):
            # Track this monitor thread
            monitor_tid = threading.get_ident()
            if self._process_tracker and self._worker_tid:
                self._process_tracker.track_monitor_thread(
                    self._worker_tid, monitor_tid, process_name, stream_name
                )

            try:
                for line in iter(stream.readline, ""):
                    if not line:
                        break
                    line = line.strip()
                    if not line:
                        continue

                    if pattern_matching:
                        with self._lldb_output_lock:
                            self._lldb_output.append(line)

                        try:
                            self._lldb_line_queue.put_nowait(line)
                        except queue.Full:
                            self.logger.warning(
                                f"LLDB queue full, may miss pattern: {line}"
                            )

                    from .utils import Colors, Logger as UtilsLogger

                    if UtilsLogger._verbose:
                        print(
                            f"{Colors.DIM}🔍 [{process_name}][{stream_name.upper()}] {line}{Colors.RESET}"
                        )

            except Exception as e:
                self.logger.verbose(f"Error reading {process_name} {stream_name}: {e}")

        threading.Thread(
            target=read_stream, args=(process.stdout, "stdout"), daemon=True
        ).start()
        threading.Thread(
            target=read_stream, args=(process.stderr, "stderr"), daemon=True
        ).start()

    def _start_debugger_monitoring(self):
        """Start monitoring debugger output in a separate thread"""
        self._start_process_monitoring(
            self.debugger_process, "Debugger", pattern_matching=False
        )

    def _disconnect_lldb_gracefully(self):
        """Gracefully disconnect LLDB before termination"""
        if self.lldb_process and self.lldb_process.poll() is None:
            try:
                # Send quit command to LLDB - no need for detach first
                self.logger.verbose("Gracefully disconnecting LLDB...")
                if hasattr(self.lldb_process, "stdin") and self.lldb_process.stdin:
                    self.lldb_process.stdin.write("quit\n")
                    self.lldb_process.stdin.flush()
            except (BrokenPipeError, OSError, ValueError):
                # LLDB process may have already terminated or stdin closed
                pass
            except Exception as e:
                self.logger.verbose(f"Exception during LLDB graceful disconnect: {e}")

    def _start_lldb_monitoring(self):
        """Start monitoring LLDB output in a separate thread with enhanced pattern detection"""
        self._start_process_monitoring(self.lldb_process, "LLDB", pattern_matching=True)

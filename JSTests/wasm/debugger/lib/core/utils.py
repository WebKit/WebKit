"""
Logging utilities and color codes for the WebAssembly Debugger Test Framework
"""

import os
import sys
import threading
from typing import TextIO


class Colors:
    """ANSI color codes for terminal output"""

    GREEN = "\033[92m"
    RED = "\033[91m"
    YELLOW = "\033[93m"
    BLUE = "\033[94m"
    CYAN = "\033[96m"
    MAGENTA = "\033[95m"
    RESET = "\033[0m"
    BOLD = "\033[1m"
    DIM = "\033[2m"


class Logger:
    """Simple, readable logging utility with optional PID/TID prefixes"""

    _verbose = False
    _enable_pid_tid = False
    _quiet_parallel = False

    @classmethod
    def set_verbose(cls, verbose: bool):
        cls._verbose = verbose

    @classmethod
    def set_pid_tid_logging(cls, enable: bool):
        cls._enable_pid_tid = enable
        if enable:
            OutputInterceptor.start_interception()
        else:
            OutputInterceptor.stop_interception()

    @classmethod
    def set_quiet_parallel(cls, enable: bool):
        cls._quiet_parallel = enable

    @classmethod
    def _get_prefix(cls) -> str:
        """Get PID/TID prefix if enabled and not intercepting"""
        if cls._enable_pid_tid and not OutputInterceptor.is_intercepting():
            return f"[PID:{os.getpid()}][TID:{threading.get_ident()}] "
        return ""

    @classmethod
    def _get_pid_tid_prefix(cls) -> str:
        """Get PID/TID prefix for OutputInterceptor (always enabled when called)"""
        return f"[PID:{os.getpid()}][TID:{threading.get_ident()}] "

    @classmethod
    def _should_show_in_quiet_mode(cls, msg: str) -> bool:
        """Simple check for important messages in quiet mode"""
        if not cls._quiet_parallel:
            return True

        # Show important results and summaries including runtime details
        important_keywords = [
            "✅ PASSED",
            "❌ FAILED",
            "Total tests:",
            "Passed:",
            "Failed:",
            "Total test execution time:",
            "Wall clock time:",
            "Parallelization speedup:",
            "Parallel efficiency:",
            "🎉",
            "📊",
            "Starting Parallel",
            "Summary",
        ]
        return any(keyword in msg for keyword in important_keywords)

    @classmethod
    def _log(cls, msg: str, color: str, icon: str):
        """Simple unified logging"""
        if cls._should_show_in_quiet_mode(msg):
            prefix = cls._get_prefix()
            print(f"{color}{icon} {prefix}{msg}{Colors.RESET}")

    @classmethod
    def success(cls, msg: str):
        cls._log(msg, Colors.GREEN, "✅")

    @classmethod
    def error(cls, msg: str):
        cls._log(msg, Colors.RED, "❌")

    @classmethod
    def warning(cls, msg: str):
        cls._log(msg, Colors.YELLOW, "⚠️")

    @classmethod
    def info(cls, msg: str):
        cls._log(msg, Colors.BLUE, "ℹ️")

    @classmethod
    def parallel_step(cls, msg: str):
        """Parallel execution steps (always shown)"""
        prefix = cls._get_prefix()
        print(f"{Colors.CYAN}🔄 {prefix}{msg}{Colors.RESET}")

    @classmethod
    def verbose(cls, msg: str):
        """Verbose output with clean bypass for PID/TID mode"""
        if cls._verbose:
            if cls._enable_pid_tid:
                print(f"{Colors.DIM}🔍 [FRAMEWORK] {msg}{Colors.RESET}")
            else:
                original_stdout = OutputInterceptor._original_stdout or sys.stdout
                original_stdout.write(
                    f"{Colors.DIM}🔍 [FRAMEWORK] {msg}{Colors.RESET}\n"
                )
                original_stdout.flush()

    @classmethod
    def debug(cls, msg: str):
        """Debug output"""
        if cls._verbose:
            cls.verbose(f"DEBUG: {msg}")

    @classmethod
    def header(cls, msg: str):
        """Header with simple filtering"""
        if cls._should_show_in_quiet_mode(msg):
            prefix = cls._get_prefix()
            print(f"\n{Colors.BOLD}{Colors.CYAN}=== {prefix}{msg} ==={Colors.RESET}")

    @classmethod
    def subheader(cls, msg: str):
        if not cls._quiet_parallel:
            prefix = cls._get_prefix()
            print(f"\n{Colors.BOLD}{Colors.MAGENTA}--- {prefix}{msg} ---{Colors.RESET}")

    @classmethod
    def cleanup_interception(cls):
        OutputInterceptor.stop_interception()


class PrefixedOutput:
    """Output stream wrapper that adds PID/TID prefixes to all lines"""

    def __init__(self, original_stream: TextIO, prefix_func):
        self.original_stream = original_stream
        self.prefix_func = prefix_func
        self._buffer = ""

    def write(self, text: str) -> int:
        """Write text with PID/TID prefix for each line"""
        if not text:
            return 0

        # Add to buffer
        self._buffer += text

        # Process complete lines
        while "\n" in self._buffer:
            line, self._buffer = self._buffer.split("\n", 1)
            if line.strip():  # Only prefix non-empty lines
                prefix = self.prefix_func()
                self.original_stream.write(f"{prefix}{line}\n")
            else:
                self.original_stream.write("\n")

        return len(text)

    def flush(self):
        """Flush any remaining buffer and the original stream"""
        if self._buffer.strip():
            prefix = self.prefix_func()
            self.original_stream.write(f"{prefix}{self._buffer}")
            self._buffer = ""
        self.original_stream.flush()

    def __getattr__(self, name):
        """Delegate other attributes to the original stream"""
        return getattr(self.original_stream, name)


class OutputInterceptor:
    """Global output interceptor for adding PID/TID prefixes to all output"""

    _original_stdout = None
    _original_stderr = None
    _intercepting = False

    @classmethod
    def start_interception(cls):
        """Start intercepting stdout/stderr to add PID/TID prefixes"""
        if cls._intercepting:
            return

        cls._original_stdout = sys.stdout
        cls._original_stderr = sys.stderr

        sys.stdout = PrefixedOutput(cls._original_stdout, Logger._get_pid_tid_prefix)
        sys.stderr = PrefixedOutput(cls._original_stderr, Logger._get_pid_tid_prefix)

        cls._intercepting = True

    @classmethod
    def stop_interception(cls):
        """Stop intercepting and restore original streams"""
        if not cls._intercepting:
            return

        # Flush any remaining output
        if hasattr(sys.stdout, "flush"):
            sys.stdout.flush()
        if hasattr(sys.stderr, "flush"):
            sys.stderr.flush()

        # Restore original streams
        sys.stdout = cls._original_stdout
        sys.stderr = cls._original_stderr

        cls._intercepting = False

    @classmethod
    def is_intercepting(cls) -> bool:
        """Check if output interception is active"""
        return cls._intercepting
